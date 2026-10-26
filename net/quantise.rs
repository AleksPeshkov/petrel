use bullet_lib::{
    game::inputs::Chess768hm,
    nn::{InitSettings, Shape, optimiser::AdamW},
    trainer::save::SavedFormat,
    value::ValueTrainerBuilder,
};

fn main() {
    const CPU_THREADS: usize = 16;
    const LOSS_POW: f32 = 2.6;

    const ACC_SIZE: usize = 1024;
    const HALF_SIZE: usize = ACC_SIZE / 2; // size of each pair
    const VECTOR_LANES: usize = 16;

    const QW0: f32 = 2048.0; // 2^11, seems safe enough for the 16-bit chess accumulator
    const QM1: f32 = 2048.0; // 2^11, balanced precision of QW0*QW0 in i16
    const WDL: f32 = 400.0; // embedded net output conversion 1.0 = 400 centipawns
    const QW1: f32 = 16.0 * WDL; // 2^4 * WDL
    const QB1: f32 = QM1 * QW1; // 2^15 * WDL

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

                            for p in 0..2 {
                                for h in 0..HALF_SIZE {
                                    // [p][h]
                                    let i = p * HALF_SIZE + h;

                                    // [n][p][lane]
                                    let n = h / VECTOR_LANES;
                                    let lane = h % VECTOR_LANES;
                                    let to_i = (2*n + p) * VECTOR_LANES + lane;

                                    // embed bias into kings weights
                                    let bias = if piece == 5 { store.get("l0b").values[i] / 2.0 } else { 0.0 };
                                    outputs[to + to_i] = inputs[from + i] + bias;
                                }
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
            l0.init_with_effective_input_size(2048);
            let my_acc = l0.forward(my_inputs).relu();
            let op_acc = l0.forward(op_inputs).relu();

            let my_diff = my_acc.slice_rows(0, HALF_SIZE) - my_acc.slice_rows(HALF_SIZE, ACC_SIZE);
            let op_diff = op_acc.slice_rows(0, HALF_SIZE) - op_acc.slice_rows(HALF_SIZE, ACC_SIZE);
            let diff_squared = my_diff.concat(op_diff).sqrrelu();

            let l1b = builder.new_weights("l1b", Shape::new(1, 1), InitSettings::Zeroed);
            let l1w = builder.new_weights("l1w", Shape::new(1, ACC_SIZE),
                InitSettings::Vector{ vector: [
                    vec![[0.25, -0.25]; HALF_SIZE/2].concat(),
                    vec![[-0.25, 0.25]; HALF_SIZE/2].concat(),
                ].concat() }
            );
            l1b + l1w.matmul(diff_squared)
        });

    trainer.load_from_checkpoint("./checkpoints/1y1-120/");
    trainer.save_to_checkpoint("./quantised/");
}
