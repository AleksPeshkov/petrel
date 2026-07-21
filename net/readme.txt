NNUE weights file (quantised.bin) embeded into application binary and created from scratch using only:

1) https://github.com/AleksPeshkov/bullet (forked from https://github.com/jw1912/bullet)
Bullet with CPU backend with Petrel specific features.

2) petrel.rs config based on https://github.com/jw1912/bullet/blob/main/examples/simple.rs and influenced by
https://github.com/linrock/minifish/blob/main/training/HL64-q96-q144-hm--S2-T77novT79-lr125--S1-pdist-no-wm-lr15.rs

3) data files from:
https://huggingface.co/datasets/linrock/bullet-training-data/blob/main/S2/
