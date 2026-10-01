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

    const ACC_SIZE: usize = 1024;

    const QW0: f32 = 1024.0; // seems safe and large enough for 16-bit accumulator
    const QS0: f32 = 2048.0; // balanced precision of QW0*QW0 in i16
    const WDL: f32 = 400.0;  // implicit output conversion 1.0 = 400 centipawns
    const QW1: f32 = 16.0 * WDL; // QW1*WDL*MW1 <= 32767
    const QB1: f32 = QS0 * QW1; // 2^15 * WDL

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0w").transform(|store, inputs| {
                let engine: [usize; 6] = [4, 3, 2, 1, 0, 5]; // pnbrqk -> qrbnpk

                let mut outputs = vec![0.0; inputs.len()];

                for side in 0..2 {
                    for piece in 0..6 {
                        for square in 0..64 {
                            let from = (side*6*64 + piece*64 + square) * ACC_SIZE;
                            // pnbrqk -> qrbnpk; A1 = 0 -> H8 = 0
                            let to = (engine[piece]*128 + side*64 + (square^63)) * ACC_SIZE;

                            for i in 0..ACC_SIZE {
                                // embed bias into kings weights
                                let bias = if piece == 5 { store.get("l0b").values[i] / 2.0 } else { 0.0 };
                                outputs[to + i] = inputs[from + i] + bias;
                            }
                        }
                    }
                }
                outputs
            }).quantise::<i16>(QW0),
            SavedFormat::id("l1w").quantise::<i16>(QW1),
            SavedFormat::id("l1b").quantise::<i32>(QB1),
        ])
        .inputs(Chess768hm).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let l0 = builder.new_affine("l0", 768, ACC_SIZE);
            let my_acc = l0.forward(my_inputs);
            let op_acc = l0.forward(op_inputs);
            let dacc = my_acc.concat(op_acc);

            let l1 = builder.new_affine("l1", 2*ACC_SIZE, 1);
            l1.forward(dacc.screlu())
        });

    // loading directly from a `BulletFormat` file
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
    let mut batch_size = 16_384 /4;
    let mut batches_per_superbatch = 6_104 *4;
    let mut initial_lr = 1e-3;
    let final_lr = 1e-6;

    const MW1: f32 = 32767.0 / QW1; // 5.11984375
    trainer.optimiser.set_params_for_weight("l0b", AdamWParams{ decay: 0.00, min_weight: -4.0, max_weight: 4.0, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l1b", AdamWParams{ decay: 0.00, min_weight: -4.0, max_weight: 4.0, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l0w", AdamWParams{ decay: 0.01, min_weight: -4.0, max_weight: 4.0, ..Default::default() });
    trainer.optimiser.set_params_for_weight("l1w", AdamWParams{ decay: 0.03, min_weight: -MW1, max_weight: MW1, ..Default::default() });

    let schedule = TrainingSchedule {
        net_id: "1x1".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch: 1, end_superbatch: final_superbatch },
        wdl_scheduler: wdl::CosineDecayWDL { start: 0.0, end: 0.10, final_superbatch },
        lr_scheduler: lr::CosineDecayLR { initial_lr, final_lr, final_superbatch },
        save_rate: 10,
    };
    //trainer.run(&schedule, &settings, &data_loader);

    batch_size *= 4;
    batches_per_superbatch /= 4;
    initial_lr /= 10.0;
    let schedule2 = TrainingSchedule {
        net_id: "1x4".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch: 1, end_superbatch: final_superbatch },
        wdl_scheduler: wdl::CosineDecayWDL { start: 0.10, end: 0.20, final_superbatch },
        lr_scheduler: lr::CosineDecayLR { initial_lr, final_lr, final_superbatch },
        save_rate: 10,
    };
    trainer.load_from_checkpoint(&format!("./{}/{}-{}", &settings.output_directory, "1x1", final_superbatch));
    trainer.run(&schedule2, &settings, &data_loader);
}
