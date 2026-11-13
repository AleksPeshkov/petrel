use bullet_lib::{
    game::inputs::Chess768hm,
    nn::optimiser::{AdamW, AdamWParams},
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

    const ACC_NEURONS: usize = 1024;

    const QW0: f32 = 1024.0; // seems safe and large enough for 16-bit accumulator
    const WDL: f32 = 300.0;  // implicit output conversion 1.0 = 400 centipawns
    const QW1: f32 = 64.0 * WDL; // 2^6 * WDL
    const QB1: f32 = 2048.0 * WDL; // 2^11 * WDL

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0w").transform(|store, inputs| {
                let mut outputs = vec![0.0; inputs.len()];
                let engine: [usize; 6] = [4, 3, 2, 1, 0, 5]; // pnbrqk -> qrbnpk

                for side in 0..2 {
                    for piece in 0..6 {
                        for square in 0..64 {
                            let from = (side*6*64 + piece*64 + square) * ACC_NEURONS;
                            // pnbrqk -> qrbnpk; A1 = 0 -> H8 = 0
                            let to = (engine[piece]*128 + side*64 + (square^63)) * ACC_NEURONS;

                            for i in 0..ACC_NEURONS {
                                // embed bias into kings weights
                                let bias = if piece == 5 { store.get("l0b").values[i] / 2.0 } else { 0.0 };
                                outputs[to + i] = inputs[from + i] + bias;
                            }
                        }
                    }
                }
                outputs
            }).quantise::<i16>(QW0),

            SavedFormat::id("l1w").rescale::<i16>(QW1).transform(|_, inputs| {
                let mut outputs = vec![0.0; inputs.len()];
                let mut odd = false; // parity count of odd weights

                for i in 0..inputs.len() {
                    // 0) we multiply positive value with signed _w_
                    // 1) _mm256_mulhrs_epi16 rounds positive product up, negative also up (towards zero)
                    // 2) rounding up happens only when _w_ lowest bit is one
                    // 3) compensate systematic odd _w_ upward error by rounding down each other odd _w_
                    let mut w = inputs[i] as i16;
                    if (w & 1) != 0 {
                        if odd { w -= 1; }
                        odd = !odd;
                    }
                    outputs[i] = w as f32;
                }
                outputs
            }).quantise_to_type::<i16>(),

            SavedFormat::id("l1b").quantise::<i32>(QB1),
        ])
        .inputs(Chess768hm).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let l0 = builder.new_affine("l0", 768, ACC_NEURONS);
            let my_acc = l0.forward(my_inputs).screlu();
            let op_acc = l0.forward(op_inputs).screlu();
            let dacc = my_acc.concat(op_acc);

            let l1 = builder.new_affine("l1", 2*ACC_NEURONS, 1);
            l1.forward(dacc)
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
    let settings = LocalSettings { threads: CPU_THREADS/2, test_set: None, output_directory: "checkpoints", batch_queue_size: 256 };

    trainer.load_from_checkpoint(&format!("./{}/{}-{}", &settings.output_directory, "1024-hm03", 360));

    let batch_size = 16_384 / 4;
    let batches_per_superbatch = 6_104 * 4;

    const MW1: f32 = 32767.0 / QW1; // 1.279960938
    trainer.optimiser.set_params_for_weight("l0w", AdamWParams{ decay: 0.01, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l1w", AdamWParams{ decay: 0.03, min_weight: -MW1, max_weight: MW1, ..Default::default() });

    let mut start_superbatch = 1;
    let mut end_superbatch = 20;
    let schedule1 = TrainingSchedule {
        net_id: "0-s1".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch, end_superbatch },
        wdl_scheduler: wdl::ConstantWDL { value: 0.0 },
        lr_scheduler: lr::ConstantLR { value: 8e-4 },
        save_rate: 10,
    };
    trainer.run(&schedule1, &settings, &data_loader);

    trainer.optimiser.set_params_for_weight("l0w", AdamWParams{ ..Default::default() });

    start_superbatch = end_superbatch + 1;
    end_superbatch = 100;
    let schedule2 = TrainingSchedule {
        net_id: "0-s2".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch, end_superbatch },
        wdl_scheduler: wdl::RangedLinearDecayWDL { initial_wdl: 0.0, final_wdl: 0.1, start_superbatch, end_superbatch },
        lr_scheduler: lr::RangedCosineDecayLR { initial_lr: 8e-4, final_lr: 2e-5, start_superbatch, end_superbatch },
        save_rate: 10,
    };
    trainer.run(&schedule2, &settings, &data_loader);

    trainer.optimiser.set_params_for_weight("l1w", AdamWParams{ min_weight: -MW1, max_weight: MW1, ..Default::default() });

    start_superbatch = end_superbatch + 1;
    end_superbatch = 120;
    let schedule3 = TrainingSchedule {
        net_id: "0-s3".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch, end_superbatch },
        wdl_scheduler: wdl::ConstantWDL { value: 0.1 },
        lr_scheduler: lr::RangedLinearDecayLR { initial_lr: 2e-5, final_lr: 4e-6, start_superbatch, end_superbatch },
        save_rate: 10,
    };
    trainer.run(&schedule3, &settings, &data_loader);
}
