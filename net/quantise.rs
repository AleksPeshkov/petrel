use bullet_lib::{
    game::inputs::Chess768hm,
    nn::optimiser::AdamW,
    trainer::save::SavedFormat,
    value::ValueTrainerBuilder,
};

fn main() {
    const CPU_THREADS: usize = 16;
    const LOSS_POW: f32 = 2.6;

    const ACC_SIZE: usize = 1024;

    const QW0: f32 = 1024.0; // seems safe and large enough for 16-bit accumulator
    const QS0: f32 = 2048.0; // balanced precision of QW0*QW0 in i16
    const WDL: f32 = 275.0;  // implicit output conversion 1.0 = 400 centipawns
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

    trainer.load_from_checkpoint("./checkpoints/1x4-120/");
    trainer.save_to_checkpoint("./quantised/");
}
