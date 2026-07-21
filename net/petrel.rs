use bullet_lib::{
    game::inputs::Chess768,
    nn::optimiser::AdamW,
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

    const QA: i16 = 256;
    const QB: i16 = 64;

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        // map output into ranges [0, 1] to fit against our labels which
        // are in the same range
        // `target` == wdl * game_result + (1 - wdl) * sigmoid(search score in centipawns / SCALE)
        // where `wdl` is determined by `wdl_scheduler`
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0w").quantise::<i16>(QA),
            SavedFormat::id("l0b").quantise::<i16>(QA),
            SavedFormat::id("l1w").quantise::<i16>(QB),
            SavedFormat::id("l1b").quantise::<i16>(QA * QB),
        ])
        // the basic `(768 -> N)x2 -> 1` inference
        .inputs(Chess768).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            const ACCUMULATOR_SIZE: usize = 128;

            let l0 = builder.new_affine("l0", 768, ACCUMULATOR_SIZE);
            let my_accumulator = l0.forward(my_inputs).screlu();
            let op_accumulator = l0.forward(op_inputs).screlu();
            let accumulator = my_accumulator.concat(op_accumulator);

            let l1 = builder.new_affine("l1", 2 * ACCUMULATOR_SIZE, 1);
            l1.forward(accumulator)
        });

    let superbatches: usize = 120;
    let eval_scale: f32 = 800.0;

    let schedule = TrainingSchedule {
        net_id: "petrel128".to_string(),
        eval_scale,
        steps: TrainingSteps { batch_size: 16_384, batches_per_superbatch: 6_104, start_superbatch: 1, end_superbatch: superbatches },
        wdl_scheduler: wdl::LinearWDL { start: 0.0, end: 0.1 },
        lr_scheduler: lr::LinearDecayLR { initial_lr: 0.001, final_lr: 0.0, final_superbatch: superbatches },
        save_rate: 10,
    };

    // loading directly from a `BulletFormat` file
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
    let settings = LocalSettings { threads: 2, test_set: None, output_directory: "checkpoints", batch_queue_size: CPU_THREADS };
    trainer.run(&schedule, &settings, &data_loader);
}
