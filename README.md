# Petrel is UCI Chess Engine

Petrel is a conventional alpha-beta search engine, but its implementation details set it apart from others.
Version 2.1 is rated 2600 Elo on the [CCRL Blitz](https://computerchess.org.uk/ccrl/404/cgi/engine_details.cgi?print=Details&each_game=1&eng=Petrel%202.1%2064-bit#Petrel_2_1_64-bit) and the [40/15](https://computerchess.org.uk/ccrl/4040/cgi/engine_details.cgi?print=Details&each_game=0&eng=Petrel%202.1%2064-bit#Petrel_2_1_64-bit) lists.

## Supported UCI options

```
option name EvalFile type string default petrel128.bin
option name Debug Log File type string default <empty>
option name Hash type spin min 2 max 16384 default 2
option name Move Overhead type spin min 0 max 10000 default 0
option name Ponder type check default false
option name UCI_Chess960 type check default false
```

The engine expects NN file (standard bullet simple architecture format) in current working directory (not nesecceralily the same as petrel application directory itself). If network data file not found, engine will not start search and reset `option name EvalFile type string default <empty>`

## Features

Petrel is relatively fast searcher:

* [**Unique position representation**](https://www.chessprogramming.org/Piece-Sets) – Neither bitboards nor mailbox based on 128-bit SIMD vectors
* **[Color-independent position](https://www.chessprogramming.org/Color_Flipping#Monochrome) and Zobrist hashing scheme** – Allows seemless position transpositions white into black and back
* [**Hyperbola Quintessence**](https://www.chessprogramming.org/Hyperbola_Quintessence) – For fast sliding piece attack generation
* [**Incrementally updated attack tables**](https://www.chessprogramming.org/Attack_and_Defend_Maps)
* **Bulk legal move generation** – Derived directly from attack tables
* **Transposition table** – Basic "always replace" scheme
* Quiescence search (without transposition table look up)
* Unorthodox search code framework without move lists

## Search

Short list of search heuristics:

* **Principal Variation Search (PVS)**
* **Quiescence search** with **SEE-based pruning** of losing captures
* **Null Move Pruning**
* **SEE-based Move Reduction** – different reduction of losing captures, unsafe and safe quiet moves
* **SEE-based Move Pruning** – *quiet* moves to unsafe squares pruned in the last 2 plies
* **Check extension** – +1 ply

## Move Ordering

Relatively sophisticated scheme:

1. All queen promotions with capture, then without
2. Good captures sorted by **MVV/LVA**
3. **Killer Move Heuristic** – 2 moves per ply
4. [**Counter Move Heuristic**](https://www.chessprogramming.org/Countermove_Heuristic) – 2 out of 4 moves in a slot
5. **Follow-up Move Heuristic**  – 2 out of 4 moves in a slot
6. **Unique Recursive Killer Move** – fresh killer immediately propogates to grandparent node to try
7. Quiet moves from **SEE-unsafe** to **safe** squares
8. Quiet moves from **safe** to **safe** squares
9. Pawn quiet moves (plus all underpromotions) – Most advanced first
10. King quiet moves
11. All bad captures – Low valued pieces first
12. All unsafe quiet moves – Low valued pieces first

## Evaluation

* Perspective [NNUE architecture](https://github.com/jw1912/bullet/blob/main/docs/1-basics.md): 768 -> (2*N) -> 1 (N=128 for version 3.0)
* Versions prior to v3.0 used simple [**PeSTO** evaluation](https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function)

---

*Aleks Peshkov, 2006 – 2025*

Many thanks to:
* Jim Ablett for [Windows, Linux and Android PGO builds](https://jim-ablett.kesug.com/).
* Linmiao Xu (Linrock), guru of NNUE training for training data and [description what he did](https://www.kaggle.com/competitions/fide-google-efficiency-chess-ai-challenge/writeups/linrock-my-solution-cfish-nnue-data-1st)
* Pawel Koziol, author of [Publius](https://github.com/nescitus/publius) for readable implementation of modern ideas including NNUE
* Robert Hyatt (bob), author of [Crafty](https://github.com/lazydroid/crafty-chess) for clean implementation of classical ideas
