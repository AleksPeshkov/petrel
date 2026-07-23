use bullet_lib::{
    game::inputs::Chess768,
    nn::{
        InitSettings::Normal, Shape,
        optimiser::{AdamW},
    },
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

    const QUANT: f64 = 181.0; // input scale 1.0 = 181, clamp(x, -181, 181)^2
    const QA: f64 = 4.0 * QUANT; // 724, engine actual activatiton: { c = (x+2)>>2, clamp(c, -181, +181)^2 }
    const QUANT_LOG: i32 = 15; // (1 << 15) = 32768 ~= 32761 (QUANT * QUANT)

    const SCALE_LOG: i32 = 4; // extra QB quantization for better precision
    const QB: f64 = (1 << (QUANT_LOG + SCALE_LOG)) as f64 / (QUANT * QUANT);
    const QWDL: f64 = 400.0; // implicit output conversion 1.0 = 400 centipawns

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        // map output into ranges [0, 1] to fit against our labels which
        // are in the same range
        // `target` == wdl * game_result + (1 - wdl) * sigmoid(search score in centipawns / SCALE)
        // where `wdl` is determined by `wdl_scheduler`
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0w").quantise::<i16>(QA),
            SavedFormat::id("l1w").quantise::<i16>(QB * QWDL),
        ])
        // the basic `(768 -> N)x2 -> 1` inference
        .inputs(Chess768).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            const ACCUMULATOR_SIZE: usize = 128;

            let l0w = builder.new_weights("l0w", Shape::new(ACCUMULATOR_SIZE, 768),
                Normal{ mean: 0.0, stdev: (2.0 / 768.0 as f32).sqrt() }
            );
            let my_accumulator = l0w.matmul(my_inputs);
            let op_accumulator = l0w.matmul(op_inputs);
            let accumulator = my_accumulator.concat(op_accumulator);

            let l1w = builder.new_weights("l1w", Shape::new(1, 2*ACCUMULATOR_SIZE),
                Normal{ mean: 0.0, stdev: (2.0 / (2*ACCUMULATOR_SIZE) as f32).sqrt() }
            );
            l1w.matmul(accumulator.polytanh())
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

    let peak_lr = 0.0003;
    let superbatches: usize = 480;

    let warmup_sb: usize = 20;
    let peak_sb: usize = 320;

    let cosine_sb: usize = 20;
    let quiet_lr: f32 = peak_lr / 5.0;

    let final_sb: usize = 20;
    let final_lr: f32 = peak_lr / 100.0;

    let schedule = TrainingSchedule {
        net_id: "polytanh480".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size: 16_384, batches_per_superbatch: 6_104, start_superbatch: 1, end_superbatch: superbatches },
        wdl_scheduler: wdl::LinearWDL { start: 0.0, end: 0.0 },
        lr_scheduler: lr::Sequence {
            first: lr::Sequence {
                first: lr::Sequence {
                    first: lr::LinearDecayLR { initial_lr: peak_lr / warmup_sb as f32, final_lr: peak_lr, final_superbatch: warmup_sb },
                    first_scheduler_final_superbatch: warmup_sb,
                    second: lr::LinearDecayLR { initial_lr: peak_lr, final_lr: peak_lr, final_superbatch: warmup_sb + peak_sb },
                },
                first_scheduler_final_superbatch: warmup_sb + peak_sb,
                second: lr::Sequence {
                    first: lr::CosineDecayLR { initial_lr: peak_lr, final_lr: quiet_lr, final_superbatch: warmup_sb + peak_sb + cosine_sb },
                    first_scheduler_final_superbatch: warmup_sb + peak_sb + cosine_sb,
                    second: lr::LinearDecayLR { initial_lr: quiet_lr, final_lr: quiet_lr, final_superbatch: superbatches - final_sb },
                },
            },
            first_scheduler_final_superbatch: superbatches - final_sb,
            second: lr::CosineDecayLR { initial_lr: quiet_lr, final_lr, final_superbatch: superbatches },
        },
        save_rate: 10,
    };

    let settings = LocalSettings { threads: 2, test_set: None, output_directory: "checkpoints", batch_queue_size: CPU_THREADS };
    trainer.run(&schedule, &settings, &data_loader);
}
