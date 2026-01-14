# Petrel is UCI Chess Engine

Petrel is a conventional alpha-beta search engine, but its implementation details set it apart from others.
Version 2.1 is rated 2600 Elo on the [CCRL Blitz](https://computerchess.org.uk/ccrl/404/cgi/engine_details.cgi?print=Details&each_game=1&eng=Petrel%202.1%2064-bit#Petrel_2_1_64-bit) and the [40/15](https://computerchess.org.uk/ccrl/4040/cgi/engine_details.cgi?print=Details&each_game=0&eng=Petrel%202.1%2064-bit#Petrel_2_1_64-bit) lists.

## Features

* [**Unique position representation**](https://www.chessprogramming.org/Piece-Sets) – Neither bitboards nor mailbox based on 128-bit SIMD vectors
* [**Hyperbola Quintessence**](https://www.chessprogramming.org/Hyperbola_Quintessence) for sliding pieces attack generation
* [**Incrementally updated attack tables**](https://www.chessprogramming.org/Attack_and_Defend_Maps)
* Fast **Simplified SEE based on attack tables**
* **Bulk legal move generation** directly from attack tables
* Unorthodox search code framework without move lists or arrays (made moves filtered out of **bitset** of remaining moves)
* Supports FRC add DFRC chess variants

## Supported UCI options

```
option name Debug Log File type string default <empty>
option name Hash type spin min 2 max 16384 default 2
option name Move Overhead type spin min 0 max 10000 default 0
option name Ponder type check default false
option name UCI_Chess960 type check default false
```

## Evaluation

Very generic and not a point of author's interest.

* Simple [**PeSTO** evaluation](https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function)

## Search

* **Principal Variation Search (PVS)**
* **Quiescence search** with **SEE pruning** of losing captures
* **SEE Reductions** – different reduction of losing captures, unsafe and safe quiet moves
* **Null Move Pruning**
* **SEE Pruning**
* **Static Null Move Pruning**
* **Razoring**
* **Check extension**

No history heuristic and thus no LMR.

## Move Ordering

Relatively sophisticated scheme:

0. Hash move
1. All queen promotions with capture, then without
2. SEE non-losing captures sorted by **MVV/LVA**
3. **Killer Move Heuristic** – 3 moves per ply
4. **Counter Move Heuristic** – 2 out of 4 moves in a slot
5. **Follow-up Move Heuristic**  – 2 out of 4 moves in a slot
6. Quiet moves from **SEE-unsafe** to **safe** squares
7. Quiet moves from **safe** to **safe** squares
8. Pawn quiet moves (plus all underpromotions) – Most advanced first
8. King quiet moves
10. SEE losing captures – Low valued pieces first
11. SEE losing quiet moves – Low valued pieces first

## Credits

* Jim Ablett for [Windows, Linux and Android PGO builds](https://jim-ablett.kesug.com/) and code improvements

---

*Aleks Peshkov, [https://github.com/AleksPeshkov/petrel](https://github.com/AleksPeshkov/petrel)*
