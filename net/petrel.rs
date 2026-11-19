use acyclib::device::tensor::rng;
use bullet_lib::{
    game::inputs::Chess768hm,
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

    const ACC_NEURONS: usize = 1024;

    const QW0: f32 = 1024.0; // seems safe and large enough for 16-bit accumulator
    const WDL: f32 = 400.0;  // implicit output conversion 1.0 = 400 centipawns
    const QW1: f32 = 32.0 * WDL; // 2^5 * WDL

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0w").transform(|_, inputs| {
                let mut outputs = vec![0.0; inputs.len()];
                let engine: [usize; 6] = [4, 3, 2, 1, 0, 5]; // pnbrqk -> qrbnpk

                for side in 0..2 {
                    for piece in 0..6 {
                        for square in 0..64 {
                            let from = (side*6*64 + piece*64 + square) * ACC_NEURONS;
                            // pnbrqk -> qrbnpk; A1 = 0 -> H8 = 0
                            let to = (engine[piece]*128 + side*64 + (square^63)) * ACC_NEURONS;

                            for i in 0..ACC_NEURONS {
                                outputs[to + i] = inputs[from + i];
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
        ])
        .inputs(Chess768hm).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let q0 = 1.0/64.0;
            let mut l0w_init = rng::vec_f32(768*ACC_NEURONS, 0.0, q0, true);
            {
                for side in 0..2 {
                    let piece = 5;
                    for square in 0..64 {
                        let fi = (side*6*64 + piece*64 + square) * ACC_NEURONS;
                        for i in 0..ACC_NEURONS {
                            // total bias 2*q0 between both kings
                            l0w_init[fi + i] += q0;
                        }
                    }
                }
            }
            let l0w = builder.new_weights("l0w", Shape::new(ACC_NEURONS, 768), InitSettings::Vector{ vector: l0w_init } );

            let my_acc = l0w.matmul(my_inputs).sqrrelu();
            let op_acc = l0w.matmul(op_inputs).sqrrelu();
            let dacc = my_acc.concat(op_acc);

            let w1 = 1.0/32.0;
            let l1w = builder.new_weights("l1w", Shape::new(1, 2*ACC_NEURONS),
                InitSettings::Vector{ vector: [
                    vec![[w1, -w1]; ACC_NEURONS/2].concat(),
                    vec![[-w1, w1]; ACC_NEURONS/2].concat(),
                ].concat() }
            );

            l1w.matmul(dacc)
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

    fn find_lr_schedules(
        cos_start_sb: usize,
        exp_start_sb: usize,
        exp_final_sb: usize,
        cos_initial_lr: f64,
        exp_initial_lr: f64,
        exp_final_lr: f64,
    ) -> (lr::RangedCosineDecayLR, lr::RangedExponentialDecayLR) {
        assert!(cos_start_sb + 1 < exp_start_sb && exp_start_sb < exp_final_sb, "Invalid superbatch bounds");
        assert!(exp_final_lr < exp_initial_lr && exp_initial_lr < cos_initial_lr, "Invalid LR values");

        // All durations (_ds) are measured from a single origin: cos_start_sb
        let exp_start_ds = exp_start_sb - cos_start_sb;

        // Normalized cosine drop ratio in [0.0, 1.0] after elapsed_ds out of period_ds.
        // Maps progress (elapsed_ds / period_ds) to phase angle in [0, pi], then computes sin^2(angle / 2).
        fn cos_decay(elapsed_ds: usize, period_ds: usize) -> f64 {
            let half_angle_rad = 0.5 * std::f64::consts::PI * (elapsed_ds as f64) / (period_ds as f64);
            half_angle_rad.sin().powi(2)
        }

        // Computes cosine LR at cos_end_ds, constrained so the curve passes through exp_initial_lr at exp_start_ds
        let cos_lr = |cos_end_ds: usize| -> f64 {
            let end_decay = cos_decay(exp_start_ds, cos_end_ds);
            cos_initial_lr - (cos_initial_lr - exp_initial_lr) / end_decay
        };

        let exp_junction_lr = exp_initial_lr * (exp_initial_lr / exp_final_lr).powf(1.0 / (exp_final_sb - exp_start_sb) as f64);
        let junction_error_lr = |cos_end_ds: usize, cos_end_lr: f64| -> f64 {
            let junction_ds = exp_start_ds - 1;
            let junction_decay = cos_decay(junction_ds, cos_end_ds);
            let cos_junction_lr = cos_initial_lr - (cos_initial_lr - cos_end_lr) * junction_decay;
            (cos_junction_lr - exp_junction_lr).abs()
        };

        // Linear search for the superbatch with the smallest error
        let mut cos_end_ds = exp_start_ds;
        let mut cos_end_lr = cos_lr(cos_end_ds);
        let mut error = junction_error_lr(cos_end_ds, cos_end_lr);
        loop {
            let next_ds = cos_end_ds + 1;
            let next_lr = cos_lr(next_ds);
            let next_error = junction_error_lr(next_ds, next_lr);

            if next_error >= error { break; }
            assert!(next_lr >= 0.0, "Impossible to solve junction: cosine decay LR cannot reach exponential decay LR");

            cos_end_ds = next_ds;
            cos_end_lr = next_lr;
            error = next_error;
        }

        let cos_lr_schedule = lr::RangedCosineDecayLR {
            start_superbatch: cos_start_sb,
            end_superbatch: cos_start_sb + cos_end_ds,
            initial_lr: cos_initial_lr as f32,
            final_lr: cos_end_lr as f32,
        };

        let exp_lr_schedule = lr::RangedExponentialDecayLR {
            start_superbatch: exp_start_sb,
            end_superbatch: exp_final_sb,
            initial_lr: exp_initial_lr as f32,
            final_lr: exp_final_lr as f32,
        };

        (cos_lr_schedule, exp_lr_schedule)
    }

    let batch_size = 16_384 / 4;
    let batches_per_superbatch = 6_104 * 4;

    const MW1: f32 = 32767.0 / QW1; // 2.559921875
    let l0w_params = AdamWParams{ decay: 0.003, beta1: 0.95, beta2: 0.9999, min_weight: -4.0, max_weight: 4.0 };
    let l1w_params = AdamWParams{ decay: 0.02,  beta1: 0.95, beta2: 0.9999, min_weight: -MW1, max_weight: MW1 };
    trainer.optimiser.set_params_for_weight("l0w", l0w_params);
    trainer.optimiser.set_params_for_weight("l1w", l1w_params);

    let start_superbatch = 1;
    let exp_start_sb = 101;
    let exp_half_life_sb = 10;
    let exp_packs = 2;
    let exp_final_sb = exp_start_sb + exp_half_life_sb * exp_packs as usize;

    let exp_initial_lr: f64 = 2.5e-5;
    let exp_final_lr = exp_initial_lr * 0.5_f64.powi(exp_packs);

    let (cos_lr_schedule, exp_lr_schedule) = find_lr_schedules(
        start_superbatch, exp_start_sb, exp_final_sb,
        8e-4, exp_initial_lr, exp_final_lr,
    );

    let cos_schedule = TrainingSchedule {
        net_id: "025".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch, end_superbatch: exp_start_sb - 1 },
        wdl_scheduler: wdl::ConstantWDL { value: 0.0 },
        lr_scheduler: cos_lr_schedule,
        save_rate: 10,
    };
    trainer.run(&cos_schedule, &settings, &data_loader);

    let exp_schedule = TrainingSchedule {
        net_id: "025".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch: exp_start_sb, end_superbatch: exp_final_sb - 1 },
        wdl_scheduler: wdl::ConstantWDL { value: 0.0 },
        lr_scheduler: exp_lr_schedule,
        save_rate: 1,
    };
    trainer.run(&exp_schedule, &settings, &data_loader);
}
