Developer's Notes About Source Code Internals
=============================================

*The text below assumes that the reader knows chess programming terminology.*

The source code may be interesting for chess programmers due to several novel chess data structures.

The engine uses neither mailbox nor bitboard board representations. The fundamental
data structure is a 16-byte vector: a vector of bytes for each chess piece on one side of the chessboard.

The engine incrementally updates the attack table using a unique data structure—
a matrix of pieces and bitboards. Each bitboard rank is stored separately in one of 8 piece vectors.
This allows a very fast implementation of the `attackTo()` function and relatively fast updates of the attack table state.
The engine uses the so-called Reversed BitBoard (aka Hyperbola Quintessence) method for generating attacks of sliding pieces (bishops, rooks, queens).

Fully legal moves matrix is generated in bulk from the attack matrix.

During the chess tree search, the engine does not distinguish between white and black playing sides.
The `PositionSide` class represents a chess side without color specificity.
Internally, squares are relative to each side's base rank, so all pawns push from RANK_2 and promote at RANK_8, both kings start at E1 square, etc.

Zobrist hashing uses only a few keys per piece type, which rotate to generate a key for each square.
Changing the position's move side to move (null move) is a byte-reversed operation. This Zobrist implementation allows
hash transpositions into different color.

Conventions Used in the Source Code
-----------------------------------

Overloaded operators:

- operator `~` is used to flip squares, bitboards, zobrist hashes, and other data structures to convert data from the opposite side point of view.
   The flip operation reverses the byte order inside bitboards and zobrist hashes, switches ranks within squares.
- operators `+` and `-` for bitsets with assertions ensuring disjoint sets;
- operator `%` is used as a shortcut for "AND NOT" bitset operations;

Abbreviations in the code:

* `Side side`: `{My, Op}` – side to move and opposite side.
* `Color color`: `{White, Black}` – rarely used, but required for correct output of internal moves in standard chess notation.
* `PieceType ty`: `{Queen = 0, Rook = 1, Bishop = 2, Knight = 3, Pawn = 4, King = 5}` – type of possible chess piece.
* `Pi pi`: Piece Index – one of 16 piece slots in a byte vector; `{TheKing = 0}` is the slot dedicated to the king.
* `PiMask`: intermediate data – piece vector of byte masks (0 or 0xFF) for selected pieces.
    Pieces are sorted so that more valuable pieces occupy lower indexes.
* `PiSquare`: stores locations of active pieces or the `0xFF` NoSquare tag.
* `PiType`: each piece type is represented as a separate bit, enabling quick grouping by criteria.
* `PiTrait`: castling and en passant statuses, plus temporary information like currently checking pieces.
* `Bb bb`: BitBoard – a well-known 64-bit bitset representing squares on the chessboard.
* `PiBbMatrix`: matrix of Pi × Bb (128 bytes), used for storing and updating piece attack information and generating moves from attacks.

Universal Chess Interface (UCI) Extensions
------------------------------------------
Engine accepts command option `--file` (`-f`) to pass UCI initial commands from a file.

* `position`: parameters `fen` or `startpos` are optional; default is reusing previous position command.
   So, `position moves e2e4` is sufficient to make the first move.
* `position` without any options displays the current position static evaluation and FEN.
* `setoption` can be abbreviated to short forms like `set hash 1g`.
  `setoption Hash` accepts sizes in bytes `b`, kibibytes `k`, mebibytes `m`, UCI default), gibibytes `g`.
* `perft N` performs PERFT to depth `N` using bulk counting and the transposition hash table.

(c) 2025, Aleks Peshkov
