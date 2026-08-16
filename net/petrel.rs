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
use std::ops::Neg;

fn main() {
    const CPU_THREADS: usize = 16;
    const LOSS_POW: f32 = 2.6;

    const ACC_SIZE: usize = 128;

    const QA: f32 = 1024.0; // seems safe and large enough for 16-bit accumulator
    const QB: f32 = 32.0;   // QB*WDL*f_wdl <= 32767
    const WDL:f32 = 400.0;  // implicit output conversion 1.0 = 400 centipawns

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0w").transform(|store, weights| {
                let engine: [usize; 6] = [4, 3, 2, 1, 0, 5];

                let mut transformed = vec![0.0; weights.len()];

                for side in 0..2 {
                    for piece in 0..6 {
                        for square in 0..64 {
                            let from = (side*6*64 + piece * 64 + square) * ACC_SIZE;
                            // pnbrqk -> qrbnpk; A1 = 0 -> H8 = 0
                            let to = (side*6*64 + engine[piece]*64 + (square^63)) * ACC_SIZE;

                            for i in 0..ACC_SIZE {
                                // embed bias into kings weights
                                let bias = if piece == 5 { store.get("l0b").values[i] / 2.0 } else { 0.0 };
                                transformed[to + i] = weights[from + i] + bias;
                            }
                        }
                    }
                }
                transformed
            }).quantise::<i16>(QA),
            SavedFormat::id("l1w").quantise::<i16>(QB*WDL),
            SavedFormat::id("l1b").quantise::<i64>(16.0*QA * QB*WDL),
        ])
        .inputs(Chess768hm).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let l0 = builder.new_affine("l0", 768, ACC_SIZE);
            let my_acc = l0.forward(my_inputs);
            let op_acc = l0.forward(op_inputs);
            let acc = my_acc.concat(op_acc);
            // Squared Concatenated ReLU
            let pos = acc.screlu();
            let neg = acc.neg().screlu();

            let l1 = builder.new_affine("l1", 4*ACC_SIZE, 1);
            l1.forward(pos.concat(neg))
        });

    trainer.optimiser.set_params_for_weight("l0w",
        AdamWParams{ decay: 0.02, min_weight: -2.0, max_weight: 2.0, ..Default::default() }
    );

    let f_wdl = 32767.0 / (QB*WDL); // 2.559921875
    trainer.optimiser.set_params_for_weight("l1w",
        AdamWParams{ decay: 0.02, min_weight: -f_wdl, max_weight: f_wdl, ..Default::default() }
    );

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

    let superbatches = 120;
    let peak_lr = 0.0004;
    let final_lr = peak_lr / 40.0;

    let schedule = TrainingSchedule {
        net_id: "hm-concatenated-screlu".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size: 4096, batches_per_superbatch: 24416, start_superbatch: 1, end_superbatch: superbatches },
        wdl_scheduler: wdl::CosineDecayWDL { start: 0.0, end: 0.1, final_superbatch: superbatches },
        lr_scheduler: lr::CosineDecayLR { initial_lr: peak_lr, final_lr, final_superbatch: superbatches },
        save_rate: 10,
    };

    let settings = LocalSettings { threads: 2, test_set: None, output_directory: "checkpoints", batch_queue_size: CPU_THREADS };
    trainer.run(&schedule, &settings, &data_loader);
}
