# Petrel is UCI Chess Engine

Petrel is a conventional alpha-beta search engine, but its implementation details set it apart from others.
Version 2.1 is rated 2600 Elo on the [CCRL Blitz](https://computerchess.org.uk/ccrl/404/cgi/engine_details.cgi?print=Details&each_game=1&eng=Petrel%202.1%2064-bit#Petrel_2_1_64-bit) and the [40/15](https://computerchess.org.uk/ccrl/4040/cgi/engine_details.cgi?print=Details&each_game=0&eng=Petrel%202.1%2064-bit#Petrel_2_1_64-bit) lists.

## Features

Petrel is relatively fast searcher:

* [**Unique position representation**](https://www.chessprogramming.org/Piece-Sets) – Neither bitboards nor mailbox based on 128-bit SIMD vectors
* **[Color-independent position](https://www.chessprogramming.org/Color_Flipping#Monochrome) and Zobrist hashing scheme** – Allows seemless position transpositions white into black and back
* [**Hyperbola Quintessence**](https://www.chessprogramming.org/Hyperbola_Quintessence) – For fast sliding piece attack generation
* [**Incrementally updated attack tables**](https://www.chessprogramming.org/Attack_and_Defend_Maps)
* **Bulk legal move generation** – Derived directly from attack tables
* **Transposition table** – Basic "always replace" scheme
* Quiescence search without transposition look up
* Unorthodox search code framework without move lists

## Search

Small number of heuristics, yet effective:

* Alpha-beta search with **Principal Variation Search (PVS)**
* **Quiescence search** with **SEE-based pruning** of losing captures
* **Null Move Pruning** – Modest reduction: `R = 2 + d/6`
* **SEE-based Move Reduction** – Reduced more for unsafe moves, down to depth 0
* **SEE-based Move Pruning** – Unsafe quiet moves pruned in the last 2 plies
* **Check extension** – +1 ply (reduction adjustments preserved)

## Move Ordering

Relatively sophisticated scheme:

1. All queen promotions
2. Good captures sorted by **MVV/LVA**
3. **Killer Move Heuristic** – 2 moves per ply
4. [**Counter Move Heuristic**](https://www.chessprogramming.org/Countermove_Heuristic) – 2 moves per slot
5. **Follow-up Move Heuristic**  – 2 moves per slot (Crafty-style)
6. **Unique Recursive Killer Move** – Child killer immediately propogates to parent node
7. Quiet moves from **SEE-unsafe** to **safe** squares
8. Quiet moves from **safe** to **safe** squares
9. Pawn quiet moves (including all underpromotions)
10. King quiet moves
11. All bad captures – Low valued pieces first
12. All unsafe quiet moves – Low valued pieces first

## Evaluation

* Simple [**PeSTO** evaluation](https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function) (planned upgrade to **NNUE** in future)

---

*Aleks Peshkov, 2006 – 2025*

Many thanks to Jim Ablett for [Windows, Linux and Android PGO builds](https://jim-ablett.kesug.com/).
