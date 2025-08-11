Developer's Notes About Source Code Internals
=============================================

*The text below assumes that the reader knows chess programming terminology.*

This engine is not strong, fast, or simple.
It is relatively stable enough to serve as a sparring opponent for a weak engine.

The source code may be interesting for chess programmers due to several novel chess data structures.

The engine uses neither mailbox nor bitboard board representations. The fundamental
data structure is a 16-byte vector: a vector of bytes for each chess piece on one side of the chessboard.

The engine incrementally updates the attack table using a unique data structure—
a matrix of pieces and bitboards. Each bitboard rank is stored separately in one of 8 piece vectors.
This allows a very fast implementation of the `attackTo()` function and relatively fast updates of the attack table state.
The engine uses the so-called Reversed BitBoard (aka Hyperbola Quintessence) method for generating attacks of sliding pieces (bishops, rooks, queens).

Fully legal moves are generated in bulk from the attack matrix.

During the chess tree search, the engine does not distinguish between white and black playing sides.
The `PositionSide` class represents a chess side without color specificity.
Internally, squares are relative to each side's base rank, so all pawns push from RANK_2 and promote at RANK_8, both kings start at E1 square, etc.

Zobrist hashing uses only a few keys per piece type, which rotate to generate a key for each square.
Changing the position's move side to move (null move) is a byte-reversed operation. This Zobrist implementation allows
hash transpositions with color changes.

Abbreviations and Conventions Used in the Source Code
-----------------------------------------------------
As a convention, the overloaded `~` operator is used to flip squares, bitboards, and other data structures from the opposite side of the move. The flipping operation reverses the byte order inside bitboards and switches ranks within squares.

Abbreviations in the code:

* `Bb bb`: BitBoard – a well-known 64-bit bitset representing squares on the chessboard
* `Pi pi`: Piece Index – one of 16 piece slots in a byte vector; `{TheKing = 0}` is the slot dedicated to the king
*    pieces are sorted so that more valuable pieces occupy lower indexes
* `PiBb`: matrix of Pi × Bb (128 bytes), used for storing and updating piece attack information and generating moves in bulk
* `Side side`: `{My, Op}` – side to move and opposite side
* `Color color`: `{White, Black}` – rarely used, but required for correct output of internal moves in UCI notation
* `PieceType ty`: `{Queen = 0, Rook = 1, Bishop = 2, Knight = 3, Pawn = 4, King = 5}` – colorless type of possible chess piece
* `PiMask`: intermediate data – piece vector of byte masks (0 or 0xFF) for selected pieces
* `PiSquare`: stores locations of active pieces or the `0xFF` NoSquare tag
* `PiType`: each piece type is represented as a separate bit, enabling quick grouping by criteria
* `PiTrait`: castling and en passant statuses, plus temporary information like currently checking pieces

* `%` operator is used as a shortcut for "AND NOT" bitset operations
* `+`, `-` operators for bitsets with assertions ensuring disjoint sets

Universal Chess Interface (UCI) Extensions
------------------------------------------
* `<position>` – `<fen>` or `<startpos>` are optional; the previous set position is assumed
  So, `<position moves e2e4>` is sufficient to make the first move
  `<position>` without options displays the current position FEN and static evaluation

* `<setoption>` can be abbreviated to short forms like `<set hash 1g>`
  `set hash` accepts sizes in bytes (b), kilobytes (k), megabytes (m, UCI default), gigabytes (g)

* `<perft N>` performs PERFT using bulk counting and a transposition hash table

Aleks Peshkov (mailto: a###s.p####v@gmail#com, telegram: @a###sp#####v)
