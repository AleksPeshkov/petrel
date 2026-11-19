use acyclib::device::tensor::rng;
use bullet_lib::{
    game::inputs::Chess768hm,
    nn::{InitSettings, Shape, optimiser::AdamW},
    trainer::save::SavedFormat,
    value::ValueTrainerBuilder,
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

    trainer.load_from_checkpoint("checkpoints/025-120/");
    trainer.save_to_checkpoint("quantised/");
}
