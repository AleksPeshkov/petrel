use bullet_lib::{
    game::inputs::Chess768hm,
    nn::{InitSettings, Shape, optimiser::AdamW},
    trainer::save::SavedFormat,
    value::{ValueTrainerBuilder},
};

fn main() {
    const CPU_THREADS: usize = 16;
    const LOSS_POW: f32 = 2.6;

    const ACC_SIZE: usize = 1024;
    const CHANNELS: usize = 128;
    const DUAL_CHANNELS: usize = 2*CHANNELS;
    const STRIDE_SIZE: usize = ACC_SIZE / CHANNELS;

    // SIMD inferenece optimization
    const VECTOR_LANES: usize = 16;
    const CHANNELS_INDEX: usize = CHANNELS / VECTOR_LANES;

    // SIMD inference quantisation
    const QW0: f32 = 1024.0; // 2^10, l0w scale, seems safe enough for the 16-bit chess accumulator
    const QW1: f32 = 4096.0; // 2^12, l1w scale, adjusted to make QB1 = QW0
    const QB1: f32 = 1024.0; // 2^10, l1b scale, adjusted to make QB1 = QW0
    const WDL: f32 = 400.0; // embedded net output conversion 1.0 = 400 centipawns
    const QW2: f32 = 16.0 * WDL; // 2^4 * WDL

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0w").transform(|store, inputs| {
                let engine: [usize; 6] = [4, 3, 2, 1, 0, 5]; // pnbrqk -> qrbnpk

                let mut outputs = vec![0.0; inputs.len()];

                for side in 0..2 {
                    for piece in 0..6 {
                        for square in 0..64 {
                            let from_idx = (side*6*64 + piece*64 + square) * ACC_SIZE;
                            // pnbrqk -> qrbnpk; A1 = 0 -> H8 = 0
                            let to_idx = (engine[piece]*128 + side*64 + (square^63)) * ACC_SIZE;

                            for ch_idx in 0..CHANNELS_INDEX {
                                for lane in 0..VECTOR_LANES {
                                    let ch = ch_idx*VECTOR_LANES + lane;
                                    for n in 0..STRIDE_SIZE {
                                        let in_idx = ch*STRIDE_SIZE + n; // [ch][n]

                                        // embed bias into kings weights
                                        let bias = if piece == 5 { store.get("l0b").values[in_idx] / 2.0 } else { 0.0 };
                                        let w = inputs[from_idx + in_idx] + bias;

                                        // [ch_idx][n][lane]
                                        let out_idx =
                                            ch_idx * STRIDE_SIZE*VECTOR_LANES
                                            + n * VECTOR_LANES
                                            + lane;
                                        outputs[to_idx + out_idx] = w;
                                    }
                                }
                            }
                        }
                    }
                }
                outputs
            }).quantise::<i16>(QW0),

            SavedFormat::id("l1w")
                .rescale::<i16>(QW1)
                .transform(|_, inputs| {
                    let mut outputs = vec![0.0; inputs.len()];

                    // parity count of odd weights per lane
                    let mut odd = [false; VECTOR_LANES];

                    for side in 0..2 {
                        for ch_idx in 0..CHANNELS_INDEX {
                            for lane in 0..VECTOR_LANES {
                                let dch = side*CHANNELS + ch_idx*VECTOR_LANES + lane;
                                for n in 0..STRIDE_SIZE {
                                    let in_idx = dch*STRIDE_SIZE + n; // [dch][n]
                                    let mut w = inputs[in_idx] as i16;

                                    // 1) _mm256_mulhrs_epi16 rounds positive product up, negative also up (towards zero)
                                    // 2) rounding up happens only when _w_ lowest bit is one
                                    // 3) compensate systematic odd _w_ upward error by rounding down each other odd _w_
                                    if (w & 1) != 0 {
                                        if odd[lane] { w -= 1; }
                                        odd[lane] = !odd[lane];
                                    }

                                    // [side][ch_idx][n][lane]
                                    let out_idx =
                                        side * ACC_SIZE
                                        + ch_idx * STRIDE_SIZE*VECTOR_LANES
                                        + n * VECTOR_LANES
                                        + lane;
                                    outputs[out_idx] = w as f32;
                                }
                            }
                        }
                    }
                    outputs
                })
                .quantise_to_type::<i16>(),

            SavedFormat::id("l1b").quantise::<i16>(QB1),
            SavedFormat::id("l2w").quantise::<i16>(QW2),
        ])
        .inputs(Chess768hm).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let l0 = builder.new_affine("l0", 768, ACC_SIZE);
            let my_acc = l0.forward(my_inputs);
            let op_acc = l0.forward(op_inputs);
            let dacc = my_acc.concat(op_acc).screlu();

            let l1w = builder.new_weights("l1w", Shape::new(DUAL_CHANNELS*STRIDE_SIZE, 1),
                InitSettings::Normal{ mean: 0.0, stdev: (2.0 / STRIDE_SIZE as f32).sqrt() }
            );
            let l1b = builder.new_weights("l1b", Shape::new(DUAL_CHANNELS, 1), InitSettings::Zeroed);

            let mut vchannels = Vec::with_capacity(DUAL_CHANNELS);
            for dch in 0..DUAL_CHANNELS {
                let start = dch * STRIDE_SIZE;
                let end = start + STRIDE_SIZE;

                let b1 = l1b.slice_rows(dch, dch+1);
                let w1 = l1w.slice_rows(start, end);
                let inputs = dacc.slice_rows(start, end);
                let channel = (b1 + w1.gemm(true, inputs, false)).screlu();

                vchannels.push(channel);
            }
            let mut dchannels = vchannels[0].clone();
            for dch in 1..DUAL_CHANNELS {
                dchannels = dchannels.concat(vchannels[dch]);
            }

            let l2w = builder.new_affine("l2w", DUAL_CHANNELS, 1);
            l2w.forward(dchannels)
        });

    trainer.load_from_checkpoint("./checkpoints/1h1-120/");
    trainer.save_to_checkpoint("./quantised/");
}
