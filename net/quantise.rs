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
    const STRIDE_SIZE: usize = 32;
    const CHANNELS: usize = ACC_SIZE / STRIDE_SIZE;
    const DUAL_CHANNELS: usize = 2*CHANNELS;

    // SIMD inferenece optimization
    const VECTOR_LANES: usize = 16;
    const CHANNELS_INDEX: usize = CHANNELS / VECTOR_LANES;

    // SIMD inference quantisation
    const QW0: f32 = 1024.0; // 2^10, l0w scale, seems safe enough for the 16-bit chess accumulator
    const QW1: f32 = 4096.0; // 2^12, l1w scale, adjusted to make QB1 = QW0
    const QB1: f32 = 1024.0; // 2^10, l1b scale, adjusted to make QB1 = QW0
    const WDL: f32 = 400.0; // embedded net output conversion 1.0 = 400 centipawns
    const QW2: f32 = 8.0 * WDL; // 2^3 * WDL

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
                                    for n in 0..STRIDE_SIZE {
                                        // [ch_idx][lane][n]
                                        let in_idx = (ch_idx*VECTOR_LANES + lane)*STRIDE_SIZE + n;

                                        // [ch_idx][n][lane]
                                        let out_idx = (ch_idx*STRIDE_SIZE + n)*VECTOR_LANES + lane;

                                        // embed bias into kings weights
                                        let bias = if piece == 5 { store.get("l0b").values[in_idx] / 2.0 } else { 0.0 };

                                        outputs[to_idx + out_idx] = inputs[from_idx + in_idx] + bias;
                                    }
                                }
                            }
                        }
                    }
                }
                outputs
            }).quantise::<i16>(QW0),

            SavedFormat::id("l1w").rescale::<i16>(QW1).transform(|_, inputs| {
                let mut outputs = vec![0.0; inputs.len()];

                // parity count of odd weights per lane
                let mut odd = [false; VECTOR_LANES];

                for side in 0..2 {
                    for ch_idx in 0..CHANNELS_INDEX {
                        let dchi = side*CHANNELS + ch_idx*VECTOR_LANES;
                        for lane in 0..VECTOR_LANES {
                            for n in 0..STRIDE_SIZE {
                                // [side][ch_idx][lane][n]
                                let in_idx = (dchi + lane)*STRIDE_SIZE + n;

                                // [side][ch_idx][n][lane]
                                let out_idx = dchi*STRIDE_SIZE + n*VECTOR_LANES + lane;

                                let mut w = inputs[in_idx] as i16;

                                // 1) _mm256_mulhrs_epi16 rounds positive product up, negative also up (towards zero)
                                // 2) rounding up happens only when _w_ lowest bit is one
                                // 3) compensate systematic odd _w_ upward error by rounding down each other odd _w_
                                if (w & 1) != 0 {
                                    if odd[lane] { w -= 1; }
                                    odd[lane] = !odd[lane];
                                }

                                outputs[out_idx] = w as f32;
                            }
                        }
                    }
                }
                outputs
            }).quantise_to_type::<i16>(),

            SavedFormat::id("l1b").quantise::<i16>(QB1),

            SavedFormat::id("l2w").transform(|_, inputs| {
                let mut outputs = vec![0.0; inputs.len()];

                for side in 0..2 {
                    for ch_idx in 0..CHANNELS_INDEX {
                        for concat in 0..2 {
                            for lane in 0..VECTOR_LANES {
                                let dchi = side*CHANNELS + ch_idx*VECTOR_LANES;

                                // [side][ch_idx][lane][concat]
                                let in_idx = (dchi + lane)*2 + concat;

                                // [side][ch_idx][concat][lane]
                                let out_idx = dchi*2 + concat*VECTOR_LANES + lane;

                                outputs[out_idx] = inputs[in_idx];
                            }
                        }
                    }
                }
                outputs
            }).quantise::<i16>(QW2),
        ])
        .inputs(Chess768hm).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let l0 = builder.new_affine("l0", 768, ACC_SIZE);
            l0.init_with_effective_input_size(2048);

            let l1b = builder.new_weights("l1b", Shape::new(DUAL_CHANNELS, 1), InitSettings::Zeroed);

            // +w, -w, +w, -w, ... inside stride pattern
            let l1w = builder.new_weights("l1w", Shape::new(2*ACC_SIZE, 1),
                InitSettings::Vector{ vector: vec![[1.0, -1.0]; ACC_SIZE].concat() }
            );

            // +w * my_acc[Pos] -w * my_acc[Neg] -w * op_acc[Pos] +w * op_acc[Neg]
            let l2w = builder.new_weights("l2w", Shape::new(2*DUAL_CHANNELS, 1),
                InitSettings::Vector{ vector: [
                    vec![[1.0, -1.0, -1.0, 1.0]; CHANNELS/2].concat(),
                    vec![[-1.0, 1.0, 1.0, -1.0]; CHANNELS/2].concat(),
                ].concat() }
            );

            let my_acc = l0.forward(my_inputs);
            let op_acc = l0.forward(op_inputs);

            let forward_channel = |dch| {
                let dch1 = dch + 1;
                let acc_stride =
                    if dch < CHANNELS {
                        my_acc.slice_rows(dch * STRIDE_SIZE, dch1 * STRIDE_SIZE)
                    } else {
                        let och = dch - CHANNELS;
                        op_acc.slice_rows(och * STRIDE_SIZE, (och+1) * STRIDE_SIZE)
                    };
                // weighted sum of Squared Clipped ReLU (c*c, c = x.clamp(0,1)) activated per channel accumulator stride
                let acc_activated = acc_stride.screlu();
                let b1 = l1b.slice_rows(dch, dch1);
                let w1 = l1w.slice_rows(dch * STRIDE_SIZE, dch1 * STRIDE_SIZE);
                let dch_input = b1 + w1.gemm(true, acc_activated, false);

                // Signed Squared Clipped ReLU (c*|c|, c = x.clamp(-1,1)) channel scalar activation
                let dch_activated = dch_input.signed_screlu();

                // weighted sum of double activated (concatenated) output { c.clamp(+1,0), c.clamp(0,-1) }
                let dch_concatenated = dch_activated.concat(-dch_activated).relu();
                let w2 = l2w.slice_rows(dch * 2, dch1 * 2);
                w2.gemm(true, dch_concatenated, false)
            };

            let mut output = forward_channel(0); // output bias = 0
            for dch in 1..DUAL_CHANNELS {
                output = output + forward_channel(dch);
            }
            output // identity activation
        });

    trainer.load_from_checkpoint("./checkpoints/1r2-120/");
    trainer.save_to_checkpoint("./quantised/");
}
