#include <set>
#include "UciRoot.hpp"

namespace {

/** Setup a chess position from a FEN string with chess legality validation */
class FenToBoard {
    struct SquareImportance {
        bool operator () (Square sq1, Square sq2) const {
            if (Rank{sq1} != Rank{sq2}) {
                return Rank{sq1} < Rank{sq2}; //Rank8 < Rank1
            }
            else {
                constexpr Rank::arrayOf<int> order{6, 4, 2, 0, 1, 3, 5, 7};
                return order[File{sq1}] < order[File{sq2}]; //FileD < FileE < FileC < FileF < FileB < FileG < FileA < FileH
            }
        }
    };
    using Squares = std::set<Square, SquareImportance>;

    Color::arrayOf< PieceType::arrayOf<Squares> > pieces;
    Color::arrayOf<int> pieceCount = {{0, 0}};

    bool drop(Color, PieceType, Square);

public:
    friend istream& read(istream&, FenToBoard&);
    bool dropPieces(Position& pos, Color colorToMove_);
};

istream& read(istream& in, FenToBoard& board) {
    File file{FileA}; Rank rank{Rank8};

    for (io::char_type c; in.get(c); ) {
        if (std::isalpha(c) && rank.isOk() && file.isOk()) {
            Color color = std::isupper(c) ? White : Black;
            c = static_cast<io::char_type>(std::tolower(c));

            PieceType ty;
            if (!ty.from_char(c) || !board.drop(color, ty, Square{file, rank})) {
                return io::fail_char(in);
            }

            ++file;
            continue;
        }

        if ('1' <= c && c <= '8' && rank.isOk() && file.isOk()) {
            //convert digit symbol to offset and skip blank squares
            auto  f = file + (c - '0');
            if (f > static_cast<int>(File::Size)) {
                return io::fail_char(in);
            }

            //avoid out of range initialization check
            file = static_cast<File::_t>(f-1);
            ++file;
            continue;
        }

        if (c == '/' && rank.isOk()) {
            ++rank;
            file = FileA;
            continue;
        }

        //end of board description field
        if (std::isblank(c)) {
            break;
        }

        //parsing error
        return io::fail_char(in);
    }

    return in;
}

bool FenToBoard::drop(Color color, PieceType ty, Square sq) {
    //the position representaion cannot hold more then 16 total pieces per color
    if (pieceCount[color] == Pi::Size) {
        io::log("#invalid fen: too many total pieces");
        return false;
    }

    //max one king per each color
    if (ty.is(King) && !pieces[color][King].empty()) {
        io::log("#invalid fen: too many kings");
        return false;
    }

    //illegal pawn location
    if (ty.is(Pawn) && (sq.on(Rank1) || sq.on(Rank8))) {
        io::log("#invalid fen: pawn on impossible rank");
        return false;
    }

    ++pieceCount[color];
    pieces[color][ty].insert(color.is(White) ? sq : ~sq);
    return true;
}

bool FenToBoard::dropPieces(Position& position, Color colorToMove_) {
    //each side should have one king
    FOR_EACH(Color, color) {
        if (pieces[color][King].empty()) {
            io::log("#invalid fen: king is missing");
            return false;
        }
    }

    Position pos;

    FOR_EACH(Color, color) {
        Side side = colorToMove_.is(color) ? My : Op;

        FOR_EACH(PieceType, ty) {
            while (!pieces[color][ty].empty()) {
                auto piece = pieces[color][ty].begin();

                if (!pos.dropValid(side, ty, *piece)) { return false; }

                pieces[color][ty].erase(piece);
            }
        }
    }

    position = pos;
    return true;
}

class BoardToFen {
    const PositionSide& whitePieces;
    const PositionSide& blackPieces;

public:
    BoardToFen (const PositionSide& w, const PositionSide& b) :
        whitePieces{w},
        blackPieces{b}
        {}

    friend ostream& operator << (ostream& out, const BoardToFen& board) {
        FOR_EACH(Rank, rank) {
            int emptySqCount = 0;

            FOR_EACH(File, file) {
                Square sq(file,rank);

                if (board.whitePieces.has(sq)) {
                    if (emptySqCount != 0) { out << emptySqCount; emptySqCount = 0; }
                    out << static_cast<io::char_type>(std::toupper( board.whitePieces.typeAt(sq).to_char() ));
                    continue;
                }

                if (board.blackPieces.has(~sq)) {
                    if (emptySqCount != 0) { out << emptySqCount; emptySqCount = 0; }
                    out << board.blackPieces.typeAt(~sq).to_char();
                    continue;
                }

                ++emptySqCount;
            }

            if (emptySqCount != 0) { out << emptySqCount; }
            if (rank != Rank1) { out << '/'; }
        }

        return out;
    }
};

class CastlingToFen {
    std::set<io::char_type> castlingSet;

    void insert(const PositionSide& positionSide, Color color, ChessVariant chessVariant) {
        for (Pi pi : positionSide.castlingRooks()) {
            io::char_type castlingSymbol;

            switch (chessVariant) {
                case Chess960:
                    castlingSymbol = File{positionSide.squareOf(pi)}.to_char();
                    break;

                case Orthodox:
                default:
                    castlingSymbol = CastlingRules::castlingSide(positionSide.kingSquare(), positionSide.squareOf(pi)).to_char();
                    break;
            }

            if (color.is(White)) { castlingSymbol = static_cast<io::char_type>(std::toupper(castlingSymbol)); }
            castlingSet.insert(castlingSymbol);
        }
    }

public:
    CastlingToFen (const PositionSide& whitePieces, const PositionSide& blackPieces, ChessVariant chessVariant) {
        insert(whitePieces, White, chessVariant);
        insert(blackPieces, Black, chessVariant);
    }

    friend ostream& operator << (ostream& out, const CastlingToFen& castling) {
        if (castling.castlingSet.empty()) { return out << '-'; }

        for (auto castlingSymbol : castling.castlingSet) {
            out << castlingSymbol;
        }
        return out;
    }
};

class EnPassantToFen {
    const PositionSide& op;
    Rank enPassantRank;

public:
    EnPassantToFen (const PositionSide& side, Color colorToMove_):
        op{side}, enPassantRank{colorToMove_.is(White) ? Rank6 : Rank3} {}

    friend ostream& operator << (ostream& out, const EnPassantToFen& enPassant) {
        if (!enPassant.op.hasEnPassant()) { return out << '-'; }

        return out << Square{enPassant.op.enPassantFile(), enPassant.enPassantRank};
    }
};

} //end of anonymous namespace

ostream& UciRoot::fen(ostream& out) const {
    const auto& whiteSidePieces = positionSide(sideOf(White));
    const auto& blackSidePieces = positionSide(sideOf(Black));

    return out << BoardToFen(whiteSidePieces, blackSidePieces)
        << ' ' << colorToMove_
        << ' ' << CastlingToFen{whiteSidePieces, blackSidePieces, chessVariant()}
        << ' ' << EnPassantToFen{OP, colorToMove_}
        << ' ' << rule50()
        << ' ' << fullMoveNumber;
}

istream& UciRoot::readMove(istream& in, Square& from, Square& to) const {
    auto before = in.tellg();
    in >> from >> to;
    if (!in) { return io::fail_pos(in, before); }

    if (colorToMove_.is(Black)) { from.flip(); to.flip(); }

    if (!MY.has(from)) { return io::fail_pos(in, before); }
    Pi pi = MY.pieceAt(from);

    //convert special moves (castling, promotion, ep) to the internal move format
    if (MY.isPawn(pi)) {
        if (from.on(Rank7)) {
            PromoType promo{Queen};
            read(in, promo);
            in.clear(); //promotion piece is optional
            to = Square{File{to}, static_cast<Rank::_t>(+promo)};
            return in;
        }

        if (from.on(Rank5) && OP.hasEnPassant() && OP.enPassantFile().is(File{to})) {
            to = Square{File{to}, Rank5};
            return in;
        }

        //else is normal pawn move
        return in;
    }

    if (pi.is(TheKing)) {
        if (MY.has(to)) { //Chess960 castling encoding
            if (!MY.isCastling(to)) { return io::fail_pos(in, before); }

            std::swap(from, to);
            return in;
        }
        if (from.is(E1) && to.is(G1)) {
            if (!MY.has(H1) || !MY.isCastling(H1)) { return io::fail_pos(in, before); }

            from = H1; to = E1;
            return in;
        }
        if (from.is(E1) && to.is(C1)) {
            if (!MY.has(A1) || !MY.isCastling(A1)) { return io::fail_pos(in, before); }

            from = A1; to = E1;
            return in;
        }
        //else is normal king move
    }

    return in;
}

void UciRoot::limitMoves(istream& in) {
    PiBbMatrix movesMatrix;
    movesMatrix.clear();
    int n = 0;

    while (in >> std::ws && !in.eof()) {
        auto before = in.tellg();

        Square from; Square to;

        if (!readMove(in, from, to) || !isLegalMove(from, to)) {
            io::log("#invalid move");
            io::fail_pos(in, before);
            return;
        }

        Pi pi = MY.pieceAt(from);
        if (!movesMatrix.has(pi, to)) {
            movesMatrix.add(pi, to);
            ++n;
        }
    }

    if (n) {
        setMoves(movesMatrix);
        in.clear();
        return;
    }

    io::fail_rewind(in);
}

void UciRoot::playMoves(istream& in) {
    repetitions.push(colorToMove_, zobrist());

    while (in >> std::ws && !in.eof()) {
        auto before = in.tellg();

        Square from; Square to;

        if (!readMove(in, from, to) || !isLegalMove(from, to)) {
            io::fail_pos(in, before);
            return;
        }

        Position::makeMove(from, to);
        generateMoves();
        colorToMove_.flip();

        if (rule50() < 1) { repetitions.clear(); }
        repetitions.push(colorToMove_, zobrist());

        if (colorToMove_.is(White)) { ++fullMoveNumber; }
    }

    repetitions.normalize(colorToMove_);
}

istream& UciRoot::readBoard(istream& in) {
    FenToBoard board;
    if (!read(in, board)) { return in; };

    in >> std::ws;
    if (in.eof()) {
        //missing board data
        return io::fail_rewind(in);
    }

    auto beforeColor = in.tellg();
    if (!read(in, colorToMove_)) { return in; }

    Position pos;
    if (!board.dropPieces(pos, colorToMove_)) { return io::fail_pos(in, beforeColor); }
    if (!pos.afterDrop()) { return io::fail_pos(in, beforeColor); }

    static_cast<Position&>(*this) = pos;
    return in;
}

bool UciRoot::setCastling(Side My, File file) {
    return MY.setValidCastling(file);
}

bool UciRoot::setCastling(Side My, CastlingSide castlingSide) {
    return MY.setValidCastling(castlingSide);
}

istream& UciRoot::readCastling(istream& in) {
    if (in.peek() == '-') { return in.ignore(); }

    for (io::char_type c; in.get(c) && !std::isblank(c); ) {
        if (std::isalpha(c)) {
            Color color = std::isupper(c) ? White : Black;
            Side side = sideOf(color);

            c = static_cast<io::char_type>(std::tolower(c));

            CastlingSide castlingSide;
            if (castlingSide.from_char(c)) {
                if (setCastling(side, castlingSide)) { continue; }
            }
            else {
                File file;
                if (file.from_char(c)) {
                    if (setCastling(side, file)) { continue; }
                }
            }
        }
        io::fail_char(in);
    }
    return in;
}

bool UciRoot::setEnPassant(File file) {
    Square to{file, Rank4};
    if (!OP.has(to)) { return false; }

    Pi victim = OP.pieceAt(to);
    if (!OP.isPawn(victim)) { return false; }

    if (OCCUPIED.has(~Square{file, Rank3})) { return false; }

    setLegalEnPassant<Op>(victim, to);
    return true;
}

istream& UciRoot::readEnPassant(istream& in) {
    if (in.peek() == '-') { return in.ignore(); }

    Square ep;

    auto beforeSquare = in.tellg();
    if (in >> ep) {
        if (!ep.on(colorToMove_.is(White) ? Rank6 : Rank3) || !setEnPassant( File{ep} )) {
            return io::fail_pos(in, beforeSquare);
        }
    }
    return in;
}

void UciRoot::readFen(istream& in) {
    in >> std::ws;
    readBoard(in);
    in >> std::ws;
    readCastling(in);
    in >> std::ws;
    readEnPassant(in);

    if (in && !in.eof()) {
        Rule50 rule50;
        in >> rule50;
        if (in) { setRule50(rule50); }

        in >> fullMoveNumber;
        in.clear(); // ignore possibly missing 'halfmove clock' and 'fullmove number' fen fields
    }

    setZobrist();
    repetitions.clear();
    repetitions.push(colorToMove_, zobrist());
    generateMoves();
}

void UciRoot::setStartpos() {
    io::istringstream startpos{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    readFen(startpos);
}
