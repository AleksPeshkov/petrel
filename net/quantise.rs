use bullet_lib::{
    game::inputs::Chess768,
    nn::optimiser::AdamW,
    trainer::save::SavedFormat,
    value::ValueTrainerBuilder,
};

fn main() {
    const CPU_THREADS: usize = 16;
    const LOSS_POW: f32 = 2.6;

    const ACC_SIZE: usize = 1024;

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
            SavedFormat::id("l1b").quantise::<i64>(QA * (QA*16.0 * QB*WDL)/32768.0), // 16384*400
        ])
        .inputs(Chess768).dual_perspective()
        .build(|builder, my_inputs, op_inputs| {
            let l0 = builder.new_affine("l0", 768, ACC_SIZE);
            let my_acc = l0.forward(my_inputs);
            let op_acc = l0.forward(op_inputs);
            let dual_acc = my_acc.concat(op_acc);

            let l1 = builder.new_affine("l1", 2*ACC_SIZE, 1);
            l1.forward(dual_acc.screlu())
        });

    trainer.load_from_checkpoint("./checkpoints/1024-240-240/");
    trainer.save_to_checkpoint("./checkpoints/1024-hrs-240/");
}
