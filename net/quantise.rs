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
    const UNIT_SIZE: usize = 2;
    const ACC_UNITS: usize = ACC_NEURONS / UNIT_SIZE; // number of double neuron activation units
    const VECTOR_LANES: usize = 16;

    const QW0: f32 = 1024.0; // seems safe and large enough for 16-bit accumulator
    const WDL: f32 = 400.0;  // implicit output conversion 1.0 = 400 centipawns
    const QW1: f32 = 32.0 * WDL; // 2^5 * WDL

    let mut trainer = ValueTrainerBuilder::default().use_threads(CPU_THREADS/2)
        .optimiser(AdamW).loss_fn(|output, target| output.sigmoid().power_error(target, LOSS_POW))
        .save_format(&[
            SavedFormat::id("l0xw").transform(|store, inputs_x| {
                let mut outputs = vec![0.0; 768*ACC_NEURONS];
                let inputs_y = &store.get("l0yw").values;
                let engine: [usize; 6] = [4, 3, 2, 1, 0, 5]; // pnbrqk -> qrbnpk

                for side in 0..2 {
                    for piece in 0..6 {
                        for square in 0..64 {
                            let units_fi = (side*6*64 + piece*64 + square) * ACC_UNITS;

                            // pnbrqk -> qrbnpk; A1 = 0 -> H8 = 0
                            let to_fi = (engine[piece]*128 + side*64 + (square^63)) * ACC_NEURONS;

                            for i in 0..ACC_UNITS {
                                let n = i / VECTOR_LANES;
                                let lane = i % VECTOR_LANES;

                                // SignalX -> u = 0
                                let to_x = (n*UNIT_SIZE + 0) * VECTOR_LANES + lane;
                                outputs[to_fi + to_x] = inputs_x[units_fi + i];

                                // SignalY -> u = 1
                                let to_y = (n*UNIT_SIZE + 1) * VECTOR_LANES + lane;
                                outputs[to_fi + to_y] = inputs_y[units_fi + i];
                            }
                        }
                    }
                }
                outputs
            }).quantise::<i16>(QW0),

            SavedFormat::id("l1w").rescale::<i16>(QW1).transform(|_, inputs| {
                let mut outputs = vec![0.0; 2*ACC_NEURONS];
                let mut odd = false;

                for side in 0..2 {
                    let side_offset = side * ACC_NEURONS;

                    for u in 0..UNIT_SIZE {
                        // encode minus sign for SignalY (u=1)
                        let sign = if u == 0 { 1.0 } else { -1.0 };

                        let from = side_offset + u*ACC_UNITS;

                        for i in 0..ACC_UNITS {
                            let n = i / VECTOR_LANES; // AccIndex
                            let lane = i % VECTOR_LANES;

                            // 0) we multiply positive value with signed _w_
                            // 1) _mm256_mulhrs_epi16 rounds positive product up, negative also up (towards zero)
                            // 2) rounding up happens only when _w_ lowest bit is one
                            // 3) compensate systematic odd _w_ upward error by rounding down each other odd _w_
                            let mut w = (inputs[from + i] * sign).round() as i16;
                            if (w & 1) != 0 {
                                if odd { w -= 1; }
                                odd = !odd;
                            }

                            let to = side_offset + (n*UNIT_SIZE + u) * VECTOR_LANES + lane;
                            outputs[to] = w as f32;
                        }
                    }
                }
                outputs
            }).quantise_to_type::<i16>(),
        ])
        .inputs(Chess768hm).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let q0 = 1.0 / 32.0;
            let phi_inv_sq = 1.0 - (5.0_f32.sqrt() - 1.0) / 2.0; // golden ratio
            let mut w0x = rng::vec_f32(768*ACC_UNITS, 0.0, q0, true);
            let mut w0y = rng::vec_f32(768*ACC_UNITS, 0.0, q0*phi_inv_sq, true);
            {
                for i in 0..(768*ACC_UNITS) { w0y[i] += w0x[i]; }

                for side in 0..2 {
                    let piece = 5; // King
                    for square in 0..64 {
                        let fi = (side*6*64 + piece*64 + square) * ACC_UNITS;
                        // embed bias into both kings squares weights
                        for i in 0..ACC_UNITS {
                            w0x[fi + i] += 2.5*q0;
                            w0y[fi + i] += 1.5*q0;
                        }
                    }
                }
            }
            let l0xw = builder.new_weights("l0xw", Shape::new(ACC_UNITS, 768), InitSettings::Vector { vector: w0x });
            let l0yw = builder.new_weights("l0yw", Shape::new(ACC_UNITS, 768), InitSettings::Vector { vector: w0y });

            // input signal
            let rmx = l0xw.matmul(my_inputs).relu();
            let rmy = l0yw.matmul(my_inputs).relu();
            let rox = l0xw.matmul(op_inputs).relu();
            let roy = l0yw.matmul(op_inputs).relu();

            // bipolar gate
            let drm = rmx - rmy;
            let dro = rox - roy;

            // gated signal
            let amx =  drm * rmx;
            let amy = -drm * rmy;
            let aox =  dro * rox;
            let aoy = -dro * roy;

            // x * relu( x-relu(y) ) = relu(x) * relu( relu(x)-relu(y) ) = relu(relu(x) * ( relu(x)-relu(y) ))
            let all = amx.concat(amy).concat(aox).concat(aoy).relu();

            let w1x = 1.0 / 8.0;
            let w1y = 1.0 / 16.0;
            let l1w = builder.new_weights("l1w", Shape::new(1, 2*ACC_NEURONS),
                InitSettings::Vector{ vector: [
                    vec![[ w1x,-w1x]; ACC_UNITS/2].concat(), // wmx
                    vec![[-w1y, w1y]; ACC_UNITS/2].concat(), // wmy
                    vec![[-w1x, w1x]; ACC_UNITS/2].concat(), // wox
                    vec![[ w1y,-w1y]; ACC_UNITS/2].concat(), // woy
                ].concat() }
            );
            l1w.matmul(all)
        });

    trainer.load_from_checkpoint("checkpoints/s-1/");
    trainer.save_to_checkpoint("quantised/");
}
