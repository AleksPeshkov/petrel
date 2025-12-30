# Petrel is UCI Chess Engine

Petrel is a conventional alpha-beta search engine, but its implementation details set it apart from others.

Petrel 3.2 is rated 3247 Elo on the [CCRL 40/15](https://computerchess.org.uk/ccrl/4040/cgi/engine_details.cgi?eng=Petrel%203.2%2064-bit) and 3334 on the [CCRL Blitz](https://computerchess.org.uk/ccrl/404/cgi/engine_details.cgi?eng=Petrel%203.2%2064-bit) lists.

Petrel 2.2 is rated 2775 Elo on the [Ultimate Bullet Classical](https://e4e6.com/) list.

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
option name EvalFile type string default <empty>
option name Debug Log File type string default <empty>
option name Hash type spin min 2 max 16384 default 2
option name Move Overhead type spin min 0 max 10000 default 0
option name Ponder type check default false
option name UCI_Chess960 type check default false
```
If EvalFile option is `<empty>`, or file not found, or has invalid size: default embedded NNUE is used.

Only parsing errors and a sparse search warnings will be written into `Debug Log File` unless `debug on` is set
then when all engine input and output will be logged.

## Command-line options

```
      -h, --help               Show this help message and exit.
      -v, --version            Display version information and exit.
      -f [FILE], --file [FILE] Read and execute UCI commands from the specified file.
```
You can provide a configuration file. This file should contain UCI commands and will be processed first before interactive input from standard input stream.

## Evaluation

Very generic and not a point of author's interest.

* Versions prior v3.0 use simple [**PeSTO** evaluation](https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function)
* Versions v3.0 and up use common bullet perspective [NNUE architecture](https://github.com/jw1912/bullet/blob/main/docs/1-basics.md):
`768 -> (2*N) -> 1 (N=128)`

## Search

* **Principal Variation Search (PVS)**
* **Quiescence search** with **SEE-based pruning** of losing captures
* **Null Move Pruning**
* **SEE-based Move Reduction** – different reduction of losing captures, unsafe and safe quiet moves
* **SEE-based Move Pruning**
* **Static Null Move Pruning**
* **Razoring**
* **Check extension**

No history heuristic and thus no LMR.

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

---

*Aleks Peshkov, 2006 – 2025*

Credits to:
* Jim Ablett for [Windows, Linux and Android PGO builds](https://jim-ablett.kesug.com/) and many code improvements
* Linmiao Xu (Linrock), guru of NNUE training for cooked data and [description what he did](https://www.kaggle.com/competitions/fide-google-efficiency-chess-ai-challenge/writeups/linrock-my-solution-cfish-nnue-data-1st)
* Pawel Koziol, author of [Publius](https://github.com/nescitus/publius) for readable implementation of modern ideas including NNUE
* Robert Hyatt (bob), author of [Crafty](https://github.com/lazydroid/crafty-chess) for clean implementation of classical ideas
