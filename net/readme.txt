NNUE weights file (quantised.bin) embeded into application binary and created from scratch using only:

1) https://github.com/jw1912/bullet
To rebuild petrel's net, you may contact the petrel's author to get the bullet CPU backend patched version.
I use bullet from the the latest main branch commit with CPU backend support:
https://github.com/jw1912/bullet/commit/feab6443fc523c9d349427bca2d5bb3c04369420

2) petrel.rs config based on https://github.com/jw1912/bullet/blob/main/examples/simple.rs and influenced by
https://github.com/linrock/minifish/blob/main/training/HL64-q96-q144-hm--S2-T77novT79-lr125--S1-pdist-no-wm-lr15.rs

3) data files from:
https://huggingface.co/datasets/linrock/bullet-training-data/blob/main/S2/
