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
    const WDL: f32 = 300.0;  // implicit output conversion 1.0 = 400 centipawns
    const QW1: f32 = 32.0 * WDL; // 2^5 * WDL
    const QB1: f32 = 1024.0 * WDL; // 2^10 * WDL

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
            let q0 = 1.0/32.0;
            let mut l0w_init = rng::vec_f32(768*ACC_NEURONS, 0.0, q0, true);
            {
                let side = 1;
                let piece = 5;
                for square in 0..64 {
                    let fi = (side*6*64 + piece*64 + square) * ACC_NEURONS;
                    for x in 0..ACC_NEURONS {
                        l0w_init[fi + x] += 2.0*q0;
                    }
                }
            }
            let l0w = builder.new_weights("l0w", Shape::new(ACC_NEURONS, 768), InitSettings::Vector{ vector: l0w_init } );
            let l0b = builder.new_weights("l0b", Shape::new(ACC_NEURONS, 1), InitSettings::Zeroed);

            let my_acc = (l0b + l0w.matmul(my_inputs)).screlu();
            let op_acc = (l0b + l0w.matmul(op_inputs)).screlu();
            let dacc = my_acc.concat(op_acc);

            let mut l1w_init = rng::vec_f32(2*ACC_NEURONS, 0.0, q0, true);
            {
                for x in 0..ACC_NEURONS/2 {
                    l1w_init[x] = l1w_init[x].abs() * if x % 2 == 0 { 1.0 } else { -1.0 };
                    l1w_init[x + ACC_NEURONS/2] = -l1w_init[x];
                }
            }
            let l1w = builder.new_weights("l1w", Shape::new(1, 2*ACC_NEURONS), InitSettings::Vector{ vector: l1w_init } );
            let l1b = builder.new_weights("l1b", Shape::new(1, 1), InitSettings::Zeroed);

            l1b + l1w.matmul(dacc)
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

    let batch_size = 16_384 / 4;
    let batches_per_superbatch = 6_104 * 4;

    const MW1: f32 = 32767.0 / QW1; // 2.559921875
    let l0b_params = AdamWParams{ decay: 0.0,  beta1: 0.90, beta2: 0.999,  min_weight: -8.0, max_weight: 8.0 };
    let l1b_params = AdamWParams{ decay: 0.0,  beta1: 0.90, beta2: 0.999,  min_weight: -1.0, max_weight: 1.0 };
    let l0w_params = AdamWParams{ decay: 0.0,  beta1: 0.90, beta2: 0.9999, min_weight: -8.0, max_weight: 8.0 };
    let l1w_params = AdamWParams{ decay: 0.01, beta1: 0.90, beta2: 0.999,  min_weight: -MW1, max_weight: MW1 };

    trainer.optimiser.set_params_for_weight("l0b", l0b_params);
    trainer.optimiser.set_params_for_weight("l1b", l1b_params);
    trainer.optimiser.set_params_for_weight("l0w", AdamWParams{ decay: 0.02, beta1: 0.8, beta2: 0.99, ..l0w_params });
    trainer.optimiser.set_params_for_weight("l1w", AdamWParams{ decay: 0.03, beta1: 0.8, beta2: 0.99, ..l1w_params });

    let mut start_superbatch = 1;
    let mut end_superbatch = 10;
    let schedule1 = TrainingSchedule {
        net_id: "1-s1".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch, end_superbatch },
        wdl_scheduler: wdl::ConstantWDL { value: 0.0 },
        lr_scheduler: lr::ConstantLR { value: 8e-4 },
        save_rate: 10,
    };
    trainer.run(&schedule1, &settings, &data_loader);

    trainer.optimiser.set_params_for_weight("l0w", l0w_params);
    trainer.optimiser.set_params_for_weight("l1w", l1w_params);

    start_superbatch = end_superbatch + 1;
    end_superbatch = 100;
    let schedule2 = TrainingSchedule {
        net_id: "1-s2".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch, end_superbatch },
        wdl_scheduler: wdl::RangedLinearDecayWDL { initial_wdl: 0.0, final_wdl: 0.1, start_superbatch, end_superbatch },
        lr_scheduler: lr::RangedCosineDecayLR { initial_lr: 8e-4, final_lr: 4e-5, start_superbatch, end_superbatch },
        save_rate: 10,
    };
    trainer.run(&schedule2, &settings, &data_loader);

    start_superbatch = end_superbatch + 1;
    end_superbatch = 120;
    let schedule3 = TrainingSchedule {
        net_id: "1-s3".to_string(),
        eval_scale: data_set_eval_scale,
        steps: TrainingSteps { batch_size, batches_per_superbatch, start_superbatch, end_superbatch },
        wdl_scheduler: wdl::ConstantWDL { value: 0.1 },
        lr_scheduler: lr::RangedLinearDecayLR { initial_lr: 4e-5, final_lr: 1e-5, start_superbatch, end_superbatch },
        save_rate: 10,
    };
    trainer.run(&schedule3, &settings, &data_loader);
}
