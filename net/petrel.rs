use bullet_lib::{
    game::inputs::Chess768hmop,
    nn::{InitSettings, Shape, optimiser::{AdamW, AdamWParams}},
    trainer::{
        save::SavedFormat,
        schedule::{TrainingSchedule, TrainingSteps, lr, wdl},
        settings::LocalSettings,
    },
    value::{ValueTrainerBuilder, loader::DirectSequentialDataLoader},
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

            SavedFormat::id("l2w").quantise::<i16>(QW2),
        ])
        .inputs(Chess768hmop).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let l0 = builder.new_affine("l0", 768, ACC_SIZE);
            l0.init_with_effective_input_size(2048);

            let l1b = builder.new_weights("l1b", Shape::new(DUAL_CHANNELS, 1), InitSettings::Zeroed);

            // +2w/-w inside stride pattern
            let l1w = builder.new_weights("l1w", Shape::new(2*ACC_SIZE, 1),
                InitSettings::Vector{ vector: vec![[0.50, -0.25]; ACC_SIZE].concat() }
            );

            // +w_my/-w_my, -w_op/+w_op channels' weights
            let l2w = builder.new_weights("l2w", Shape::new(DUAL_CHANNELS, 1),
                InitSettings::Vector{ vector: [
                    vec![[1.0, -1.0]; CHANNELS/2].concat(),
                    vec![[-1.0, 1.0]; CHANNELS/2].concat(),
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
                // Squared Clipped ReLU (x.clamp(0,1)^2) activated per channel accumulator stride
                let acc_activated = acc_stride.screlu();

                // weighted sum of the channel stride
                let b1 = l1b.slice_rows(dch, dch1);
                let w1 = l1w.slice_rows(dch * STRIDE_SIZE, dch1 * STRIDE_SIZE);
                let dch_input = b1 + w1.gemm(true, acc_activated, false);

                // Squared Clipped ReLU (x.clamp(0,1)^2) channel scalar activation
                let dch_activated = dch_input.screlu();

                // weighted sum of channels
                let w2 = l2w.slice_rows(dch, dch1);
                w2.gemm(true, dch_activated, false)
            };

            let mut output = forward_channel(0); // output bias = 0
            for dch in 1..DUAL_CHANNELS {
                output = output + forward_channel(dch);
            }
            output // identity activation
        });

    let data_set_eval_scale: f32 = 800.0;
    let data_set: &[&str] = &[
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-1.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-2.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-3.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-4.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-5.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-6.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-7.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-8.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-9.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-10.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-11.bullet.bin",
        "data/test77nov-unfilt-test79-maraprmay-v6-dd.skip-see-ge0.wdl-pdist.iter-12.bullet.bin",
    ];
    let data_loader = DirectSequentialDataLoader::new(data_set);
    let settings = LocalSettings { threads: CPU_THREADS/2, test_set: None, output_directory: "checkpoints", batch_queue_size: CPU_THREADS*4 };

    let final_superbatch = 120;
    let batch_size = 16_384 /4;
    let batches_per_superbatch = 6_104 *4;
    let final_lr = 1e-6;

    const MW1:f32 = 32767.0 / QW1; // 7.999755859
    const MW2:f32 = 32767.0 / QW2; // 10.2396875
    trainer.optimiser.set_params_for_weight("l0b", AdamWParams{ decay: 0.0,  min_weight: -4.0, max_weight: 4.0, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l1b", AdamWParams{ decay: 0.0,  min_weight: -4.0, max_weight: 4.0, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l0w", AdamWParams{ decay: 0.01, min_weight: -4.0, max_weight: 4.0, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l1w", AdamWParams{ decay: 0.02, min_weight: -MW1, max_weight: MW1, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l2w", AdamWParams{ decay: 0.01, min_weight: -MW2, max_weight: MW2, ..Default::default() });

    let schedule = TrainingSchedule {
        net_id: "1p1".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch: 1, end_superbatch: final_superbatch },
        wdl_scheduler: wdl::CosineDecayWDL { start: 0.0, end: 0.0, final_superbatch },
        lr_scheduler: lr::CosineDecayLR { initial_lr: 8e-4, final_lr, final_superbatch },
        save_rate: 10,
    };
    trainer.run(&schedule, &settings, &data_loader);
}
