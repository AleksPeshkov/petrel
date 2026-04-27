#include <set>
#include <utility>
#include "perft.hpp"
#include "search.hpp"
#include "System.hpp"
#include "Uci.hpp"

namespace io {

ostream& app_version(ostream& os) {
    os << "petrel";

#ifdef VERSION
        os << ' ' << VERSION;
#endif

#ifdef GIT_DATE
    os << ' ' << GIT_DATE;
#else
    char year[] {__DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], '\0'};

    char month[] {
        (__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't') ? '1' :
        (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') ? '1' :
        (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c') ? '1' : '0',

        (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n') ? '1' :
        (__DATE__[0] == 'F' && __DATE__[1] == 'e' && __DATE__[2] == 'b') ? '2' :
        (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r') ? '3' :
        (__DATE__[0] == 'A' && __DATE__[1] == 'p' && __DATE__[2] == 'r') ? '4' :
        (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y') ? '5' :
        (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n') ? '6' :
        (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l') ? '7' :
        (__DATE__[0] == 'A' && __DATE__[1] == 'u' && __DATE__[2] == 'g') ? '8' :
        (__DATE__[0] == 'S' && __DATE__[1] == 'e' && __DATE__[2] == 'p') ? '9' :
        (__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't') ? '0' :
        (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') ? '1' :
        (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c') ? '2' : '0',

        '\0'
    };

    char day[] {((__DATE__[4] == ' ') ? '0' : __DATE__[4]), __DATE__[5], '\0'};

    os << ' ' << year << '-' << month << '-' << day;
#endif

#ifdef GIT_ORIGIN
        os << ' ' << GIT_ORIGIN;
#endif

#ifdef GIT_SHA
        os << ' ' << GIT_SHA;
#endif

#ifndef NDEBUG
        os << " DEBUG";
#endif

    return os;
}

istream& fail(istream& is) {
    is.setstate(std::ios::failbit);
    return is;
}

istream& fail_char(istream& is) {
    is.unget();
    return fail(is);
}

istream& fail_pos(istream& is, std::streampos here) {
    is.clear();
    is.seekg(here);
    return fail(is);
}

istream& fail_rewind(istream& is) {
    return fail_pos(is, 0);
}

/// @brief Matches a case-insensitive token pattern in the input stream, ignoring whitespace.
/// @param in Input stream to read from.
/// @param token Token pattern to match (case-insensitive). May contain multi-word tokens
///              (e.g., "setoption name UCI_Chess960 value true").
/// @retval true Fully matches the token pattern, advancing the stream past the matched tokens.
/// @retval false Fails to match. Leaves the stream unchanged. Does not change the failbit.
bool consume(istream& is, czstring token) {
    if (token == nullptr) { token = ""; }

    auto state = is.rdstate();
    auto before = is.tellg();

    using std::isspace;
    do {
        // skip leading whitespace
        while (isspace(*token)) { ++token; }
        is >> std::ws;

        // case insensitive match each character until end of token string (whitespace or \0)
        while (!isspace(*token)
            && std::tolower(*token) == std::tolower(is.peek())
        ) {
            ++token;
            is.ignore();
        }
    // continue matching the next word in the token (if present)
    } while (isspace(*token) && isspace(is.peek()));

    // ensure the token and the last stream word ended at the same time
    if (*token == '\0'
        && (isspace(is.peek()) || is.eof())
    ) {
        // success: stream is advanced past the matched token
        return true;
    }

    // failure: restore stream state
    is.seekg(before);
    is.clear(state);
    return false;
}

bool hasMore(istream& is) {
    is >> std::ws;
    return !is.eof();
}

} // namespace io

namespace { // anonymous namespace

static constexpr size_t mebibyte = 1024 * 1024;

template <typename T> static T mebi(T bytes) { return bytes / mebibyte; }
template <typename T> static constexpr T permil(T n, T m) { return (n * 1000) / m; }

template <typename T> concept Formattable = requires(const T& t, ostream& os) { t.format(os); };
template <Formattable F> ostream& operator<<(ostream& os, const F& formattable) { formattable.format(os); return os; }

struct Sep {
    u64_t v;
    ostream& format(ostream& os) const {
        if (v == 0) return os << '0';

        char buf[28];
        int idx = 27;
        buf[idx--] = '\0';

        int digitCount = 0;
        u64_t n = v;
        do {
            if (digitCount != 0 && digitCount % 3 == 0) {
                buf[idx--] = '\'';
            }
            buf[idx--] = '0' + (n % 10);
            n /= 10;
            ++digitCount;
        } while (n);

        return os << &buf[idx + 1];
    }
};

// trim trailing whitespace
void rtrim(std::string& str) {
    // Define the set of whitespace characters to remove (space, newline, carriage return, tab, etc.)
    const std::string whitespace = " \n\r\t\f\v";

    // Find the position of the last non-whitespace character
    size_t last_non_space = str.find_last_not_of(whitespace);

    // If a non-whitespace character is found, erase everything after it
    if (std::string::npos != last_non_space) {
        str.erase(last_non_space + 1);
    } else {
        // If the string contains only whitespace (or is empty), clear the string
        str.clear();
    }
}

// std::ostringstream output buffer, flushed on destruction
class Output : public io::ostringstream {
protected:
    const Uci& uci;
    bool flush_;
public:
    Output (const Uci* u, bool flush = true) : io::ostringstream{}, uci{*u}, flush_{flush} {}
    io::ostringstream& flush(bool _flush = true) { uci.output(str(), _flush); str(""); return *this; }
    ~Output () { flush(flush_); }
};

// std::ostringstream UciMove capable output buffer, flushed on destruction
class UciOutput : public Output {
    Color colorToMove;
public:
    UciOutput(const Uci* u, bool flush = true) : Output{u, flush}, colorToMove{uci.colorToMove()} {}
    ChessVariant chessVariant() const { return uci.chessVariant(); }
    Color color() const { return colorToMove; }
    Color flipColor() { return Color{colorToMove.flip()}; }
    void resetRootColor() { colorToMove = uci.colorToMove(); }
};

// typesafe operator<< chaining
UciOutput& operator << (UciOutput& ob, io::czstring message) {
    static_cast<ostream&>(ob) << message; return ob;
}

// convert move to UCI format
UciOutput& operator << (UciOutput& ob, HistoryMove move) {
    bool isWhite{ob.color().is(White)};
    ob.flipColor();
    ob << ' ';

    if (move.none()) {
        io::info("illegal move 0000 printed");
        ob << "0000";
        return ob;
    }

    Square from{move.from()};
    Square to{move.to()};

    Square uciFrom{isWhite ? from : ~from};
    Square uciTo{isWhite ? to : ~to};

    if (!move.historyType().is(HistoryPawn)) {
        ob << uciFrom << uciTo;
        return ob;
    }

    // pawn promotion
    if (from.on(Rank7)) {
        // the type of a promoted pawn piece encoded in place of move to's rank
        uciTo = Square{File{to}, isWhite ? Rank8 : Rank1};
        ob << uciFrom << uciTo << PromoType{::promoTypeFrom(Rank{to})};
        return ob;
    }

    // en passant capture
    if (from.on(Rank5) && to.on(Rank5)) {
        // en passant capture move internally encoded as pawn captures pawn
        ob << uciFrom << Square{File{to}, isWhite ? Rank6 : Rank3};
        return ob;
    }

    //TRICK: castling is "pawn" move as it needs special handling
    if (from.on(Rank1)) {
        // castling move internally encoded as the rook captures own king

        if (ob.chessVariant().is(Orthodox)) {
            if (from.on(FileA)) { ob << uciTo << Square{File{FileC}, Rank{uciFrom}}; return ob; }
            if (from.on(FileH)) { ob << uciTo << Square{File{FileG}, Rank{uciFrom}}; return ob; }
        }

        // Chess960:
        ob << uciTo << uciFrom;
        return ob;
    }

    // all the rest pawn moves have MoveType::Special
    ob << uciFrom << uciTo;
    return ob;
}

UciOutput& operator << (UciOutput& ob, const PrincipalVariation& pv) {
    ob << pv.score();
    auto moves = pv.moves();
    if (moves->none()) { return ob; } // empty PV (no legal moves at root)

    {
        ob << " pv";
        for (HistoryMove move; (move = *moves++).any(); ) {
            ob << move;
        }
        ob.resetRootColor();
    }
    return ob;
}

class BoardToFen {
    const PositionSide& whitePieces;
    const PositionSide& blackPieces;

public:
    BoardToFen (const PositionSide& w, const PositionSide& b) :
        whitePieces{w},
        blackPieces{b}
        {}

    friend ostream& operator << (ostream& os, const BoardToFen& board) {
        for (auto rank : range<Rank>()) {
            int emptySqCount = 0;

            for (auto file : range<File>()) {
                Square sq{file, rank};

                if (board.whitePieces.has(sq)) {
                    if (emptySqCount != 0) { os << emptySqCount; emptySqCount = 0; }
                    os << static_cast<io::char_type>(std::toupper( PieceType{board.whitePieces.typeAt(sq)}.to_char() ));
                    continue;
                }

                if (board.blackPieces.has(~sq)) {
                    if (emptySqCount != 0) { os << emptySqCount; emptySqCount = 0; }
                    os << board.blackPieces.typeAt(~sq);
                    continue;
                }

                ++emptySqCount;
            }

            if (emptySqCount != 0) { os << emptySqCount; }
            if (!rank.is(Rank1)) { os << '/'; }
        }

        return os;
    }
};

class CastlingToFen {
    std::set<io::char_type> castlingSet;

    void insert(const PositionSide& positionSide, Color::_t color, ChessVariant chessVariant) {
        for (Pi pi : positionSide.castlingRooks()) {
            io::char_type castlingSymbol{};

            switch (chessVariant.v()) {
                case Chess960:
                    castlingSymbol = File{positionSide.sq(pi)}.to_char();
                    break;

                case Orthodox:
                default:
                    castlingSymbol = CastlingRules::castlingSide(positionSide.sqKing(), positionSide.sq(pi)).to_char();
                    break;
            }

            if (color == White) { castlingSymbol = static_cast<io::char_type>(std::toupper(castlingSymbol)); }
            castlingSet.insert(castlingSymbol);
        }
    }

public:
    CastlingToFen (const PositionSide& whitePieces, const PositionSide& blackPieces, ChessVariant chessVariant) {
        insert(whitePieces, White, chessVariant);
        insert(blackPieces, Black, chessVariant);
    }

    friend ostream& operator << (ostream& os, const CastlingToFen& castling) {
        if (castling.castlingSet.empty()) { return os << '-'; }

        for (auto castlingSymbol : castling.castlingSet) {
            os << castlingSymbol;
        }
        return os;
    }
};

class EnPassantToFen {
    const PositionSide& op;
    Rank enPassantRank;

public:
    EnPassantToFen (const PositionSide& side, Color colorToMove_):
        op{side}, enPassantRank{colorToMove_.is(White) ? Rank6 : Rank3} {}

    friend ostream& operator << (ostream& os, const EnPassantToFen& enPassant) {
        if (!enPassant.op.hasEnPassant()) { return os << '-'; }

        return os << Square{enPassant.op.fileEnPassant(), enPassant.enPassantRank};
    }
};

UciOutput& operator << (UciOutput& ob, const UciPosition& pos) {
    ::fen(ob, ob.chessVariant(), pos, ob.color(), pos.fullMoveNumber());
    return ob;
}

UciOutput& info_fen(UciOutput& ob, const UciPosition& pos) {
    ob << "info" << pos.evaluate();
    ob << " fen " << pos;
    return ob;
}

UciOutput& info_pv(UciOutput& ob, const PrincipalVariation& pv, const UciSearchLimits& limits) {
    ob << "info depth " << pv.depth();
    limits.nps(ob);
    ob << pv;
    return ob;
}

istream& operator >> (istream& is, Square& sq) {
    auto before = is.tellg();

    File file; Rank rank;
    is >> file >> rank;

    if (!is) { return io::fail_pos(is, before); }

    sq = Square{file, rank};
    return is;
}

/** Setup a chess position from a FEN string with chess legality validation */
class FenToBoard {
    struct SquareImportance {
        bool operator () (Square sq1, Square sq2) const {
            if (Rank{sq1} != Rank{sq2}) {
                return Rank{sq1} < Rank{sq2}; //Rank8 < Rank1
            }
            else {
                // FileD > FileE > FileC > FileF > FileB > FileG > FileA > FileH
                // order gains a few Elo
                constexpr File::arrayOf<int> order{6, 4, 2, 0, 1, 3, 5, 7};
                return order[File{sq1}] < order[File{sq2}];
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

istream& read(istream& is, FenToBoard& board) {
    File file{FileA}; Rank rank{Rank8};

    for (io::char_type c{}; is.get(c); ) {
        if (std::isalpha(c) && rank.isOk() && file.isOk()) {
            Color color{std::isupper(c) ? White : Black};
            c = static_cast<io::char_type>(std::tolower(c));

            PieceType ty{Queen};
            if (!ty.from_char(c) || !board.drop(color, ty, Square{file, rank})) {
                return io::fail_char(is);
            }

            ++file;
            continue;
        }

        if ('1' <= c && c <= '8' && rank.isOk() && file.isOk()) {
            //convert digit symbol to offset and skip blank squares
            auto  f = file.v() + (c - '0');
            if (f > static_cast<int>(File::Size)) {
                return io::fail_char(is);
            }

            //avoid out of range initialization check
            file = File{static_cast<File::_t>(f-1)};
            ++file;
            continue;
        }

        if (c == '/' && rank.isOk()) {
            ++rank;
            file = File{FileA};
            continue;
        }

        //end of board description field
        if (std::isblank(c)) {
            break;
        }

        //parsing error
        return io::fail_char(is);
    }

    return is;
}

bool FenToBoard::drop(Color color, PieceType ty, Square sq) {
    //the position representaion cannot hold more then 16 total pieces per color
    if (pieceCount[color] == Pi::Size) {
        io::error("invalid fen: too many total pieces");
        return false;
    }

    //max one king per each color
    if (ty.is(King) && !pieces[color][PieceType{King}].empty()) {
        io::error("invalid fen: too many kings");
        return false;
    }

    //illegal pawn location
    if (ty.is(Pawn) && (sq.on(Rank1) || sq.on(Rank8))) {
        io::error("invalid fen: pawn on impossible rank");
        return false;
    }

    ++pieceCount[color];
    pieces[color][ty].insert(color.is(White) ? sq : ~sq);
    return true;
}

bool FenToBoard::dropPieces(Position& position, Color colorToMove_) {
    //each side should have one king
    for (auto color : range<Color>()) {
        if (pieces[color][PieceType{King}].empty()) {
            io::error("invalid fen: king is missing");
            return false;
        }
    }

    Position pos;
    pos.clear();

    for (auto color : range<Color>()) {
        Side side{colorToMove_.is(color) ? My : Op};

        for (auto ty : range<PieceType>()) {
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

} // anonymous namespace

ostream& fen(ostream& os, ChessVariant chessVariant, const Position& pos, Color colorToMove, int fullMoveNumber) {
    const auto& whitePieces = pos.positionSide(Side{colorToMove.is(White) ? My : Op});
    const auto& blackPieces = pos.positionSide(Side{colorToMove.is(Black) ? My : Op});

    return os << BoardToFen(whitePieces, blackPieces)
        << ' ' << colorToMove
        << ' ' << CastlingToFen{whitePieces, blackPieces, chessVariant}
        << ' ' << EnPassantToFen{pos.positionSide(Side{Op}), colorToMove}
        << ' ' << pos.rule50()
        << ' ' << fullMoveNumber;
}

istream& UciPosition::readMove(istream& is, Square& from, Square& to) const {
    auto before = is.tellg();
    is >> from >> to;
    if (!is) { return io::fail_pos(is, before); }

    if (colorToMove_.is(Black)) { from.flip(); to.flip(); }

    if (!MY.has(from)) { return io::fail_pos(is, before); }

    //convert special moves (castling, promotion, ep) to the internal move format
    if (MY.isPawn(from)) {
        if (from.on(Rank7)) {
            PromoType promo{Queen};
            is >> promo;
            is.clear(); //promotion piece is optional
            to = Square{File{to}, ::rankOf(promo)};
            return is;
        }

        if (from.on(Rank5) && OP.hasEnPassant() && OP.fileEnPassant().is(File{to})) {
            to = Square{File{to}, Rank5};
            return is;
        }

        //else is normal pawn move
        return is;
    }

    if (MY.isKing(from)) {
        if (MY.has(to)) { //Chess960 castling encoding
            if (!MY.isCastling(to)) { return io::fail_pos(is, before); }

            std::swap(from, to);
            return is;
        }
        if (from.is(E1) && to.is(G1)) {
            if (!MY.has(Square{H1}) || !MY.isCastling(Square{H1})) { return io::fail_pos(is, before); }

            from = Square{H1}; to = Square{E1};
            return is;
        }
        if (from.is(E1) && to.is(C1)) {
            if (!MY.has(Square{A1}) || !MY.isCastling(Square{A1})) { return io::fail_pos(is, before); }

            from = Square{A1}; to = Square{E1};
            return is;
        }
        //else is normal king move
    }

    return is;
}

void UciPosition::limitMoves(istream& is) {
    PiBbMatrix movesMatrix;
    movesMatrix.clear();
    int rootMoves = 0;

    while (is >> std::ws && !is.eof()) {
        auto before = is.tellg();

        Square from; Square to;

        if (!readMove(is, from, to) || !isPossibleMove(from, to)) {
            io::error("invalid move");
            io::fail_pos(is, before);
            return;
        }

        Pi pi = MY.pi(from);
        if (!movesMatrix.has(pi, to)) {
            movesMatrix.add(pi, to);
            ++rootMoves;
        }
    }
    rootMoves_ = rootMoves;

    if (rootMoves == 0) {
        io::error("go searchmoves: 0 moves");
        io::fail_rewind(is);
        return;
    }

    setMoves(movesMatrix);
    is.clear();
}

void UciPosition::playMoves(istream& is, Repetitions& repetitions) {
    repetitions.push(colorToMove_, z());

    while (is >> std::ws && !is.eof()) {
        auto before = is.tellg();

        Square from; Square to;

        if (!readMove(is, from, to) || !isPossibleMove(from, to)) {
            io::fail_pos(is, before);
            return;
        }

        Position::makeMove(from, to);
        generateMoves();
        colorToMove_.flip();

        if (rule50() == 0_ply) { repetitions.clear(); }
        repetitions.push(colorToMove_, z());

        if (colorToMove_.is(White)) { ++fullMoveNumber_; }
    }

    repetitions.normalize(colorToMove_);
}

istream& UciPosition::readBoard(istream& is) {
    FenToBoard board;
    if (!read(is, board)) { return is; };

    is >> std::ws;
    if (is.eof()) {
        //missing board data
        return io::fail_rewind(is);
    }

    auto beforeColor = is.tellg();
    is >> colorToMove_;
    if (!is) { return is; }

    Position pos;
    if (!board.dropPieces(pos, colorToMove_)) { return io::fail_pos(is, beforeColor); }
    if (!pos.afterDrop()) { return io::fail_pos(is, beforeColor); }

    static_cast<Position&>(*this) = pos;
    return is;
}

istream& UciPosition::readCastling(istream& is) {
    if (is.peek() == '-') { return is.ignore(); }

    for (io::char_type c{}; is.get(c) && !std::isblank(c); ) {
        if (std::isalpha(c)) {
            Color color{std::isupper(c) ? White : Black};
            Side side = sideOf(color.v());

            c = static_cast<io::char_type>(std::tolower(c));

            CastlingSide castlingSide;
            if (castlingSide.from_char(c)) {
                if (positionSide(side).setValidCastling(castlingSide)) { continue; }
            }
            else {
                File file;
                if (file.from_char(c)) {
                    if (positionSide(side).setValidCastling(file)) { continue; }
                }
            }
        }
        io::fail_char(is);
    }
    return is;
}

istream& UciPosition::readEnPassant(istream& is) {
    if (is.peek() == '-') { return is.ignore(); }

    Square ep;

    auto beforeSquare = is.tellg();
    if (is >> ep) {
        if (!ep.on(colorToMove_.is(White) ? Rank6 : Rank3) || !setEnPassant( File{ep} )) {
            return io::fail_pos(is, beforeSquare);
        }
    }
    return is;
}

void UciPosition::readFen(istream& is) {
    is >> std::ws;
    readBoard(is);
    is >> std::ws;
    readCastling(is);
    is >> std::ws;
    readEnPassant(is);

    if (is && !is.eof()) {
        Rule50 rule50;
        is >> rule50;
        if (is) { setRule50(rule50); }

        is >> fullMoveNumber_;
        is.clear(); // ignore possibly missing 'halfmove clock' and 'fullmove number' fen fields
    }

    setZobrist();
    generateMoves();
}

void UciPosition::setStartpos() {
    io::istringstream startpos{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    readFen(startpos);
    rootMoves_ = 20;
}

// fast exit: return the first legal move found
HistoryMove UciPosition::firstRootMove() const {
    for (Pi pi : MY.pieces()) {
        if (bbMovesOf(pi).none()) { continue; }
        return historyMove(MY.sq(pi), bbMovesOf(pi).first(), CanBeKiller::No);
    }
    return {};
}

Uci::Uci(ostream &o) :
    tt(16 * mebibyte),
    out_{o},
    bestmove_(sizeof("bestmove a7a8q ponder h2h1q"), '\0'),
    logStartTime{::timeNow()},
    pid_{System::getPid()},
    inputLine{std::string(2048, '\0')} // preallocate 2048 bytes (~200 full moves)
{
    bestmove_.clear();
    inputLine.clear();
    setEmbeddedEval();
}

void Uci::newGame() {
    limits.newGame();
    tt.newGame();
    counterMove.clear();
    followMove.clear();
}

void Uci::newSearch() {
    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (!bestmove_.empty()) {
            error("New search started, but bestmove is not empty: " + bestmove_);
            bestmove_.clear();
        }
    }
    limits.newSearch();
    tt.newSearch();
    pv.clear();
    rootBestMoves = {};
}

void Uci::newIteration() const {
    tt.newIteration();
}

void Uci::output(std::string_view message, bool flush) const {
    if (message.empty()) { return; }

    {
        std::lock_guard<decltype(outMutex)> lock{outMutex};
        out_ << message << '\n';
        if (flush) { out_.flush(); }

        if (debugOn_ && !logFileName.empty()) { _log('<', message, flush); }
    }
}

void Uci::_log(io::char_type tag, std::string_view message, bool flush) const {
    assert (!logFileName.empty() && logFile.is_open());

    if (!logFile && !logFileName.empty()) {
        // recover if the logFile is in a bad state
        logFile.clear();
        logFile.close();
        logFile.open(logFileName, std::ios::app);
        if (!logFile.is_open()) { return; }
    }

    auto timeStamp = ::elapsedSince(logStartTime);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeStamp).count();
    auto us = (std::chrono::duration_cast<std::chrono::microseconds>(timeStamp) % 1000).count();

    // format multi-line messages:

    size_t start = 0;
    while (start < message.size()) {
        auto end = message.find('\n', start);
        if (end == std::string_view::npos) { end = message.size(); }

        logFile << pid_
            << " " << ms << '.' << std::setfill('0') << std::setw(3) << us
            << " " << tag
            << message.substr(start, end - start)
            << '\n'
        ;
        start = end + 1; // skip '\n'
    }

    if (flush) { logFile.flush(); }
}

void Uci::log(io::char_type tag, std::string_view message) const {
    if (!logFileName.empty()) {
        std::lock_guard<decltype(outMutex)> lock{outMutex};
        _log(tag, message);
    }
}

void Uci::error(std::string_view message) const {
    UciOutput ob{this};
    ob << "petrel " << pid_ << " " << message << '\n';
    ob << "root fen " << position_ << " node " << limits.getNodes() << " depth " << pv.depth();
    ob << pv << '\n';

#ifndef NDEBUG
    if (!debugPosition.empty()) { ob << debugPosition << '\n'; }
    if (!debugGo.empty()) { ob << debugGo << '\n'; }
#endif

    std::cerr << ob.str() << std::flush;
    log('!', ob.str());

    ob.str("");
}

void Uci::info(std::string_view message) const {
    if (message.empty()) { return; }
    log('*', message);
}

void Uci::processInput(istream& is) {
    std::string currentLine(2048, '\0'); // preallocate 2048 bytes (~200 full moves)
    while (std::getline(is, currentLine)) {
        if (debugOn_) { log('>', currentLine); }

        inputLine.clear(); //clear previous errors
        inputLine.str(currentLine);
        inputLine >> std::ws;

        if      (consume("go"))        { go(); }
        else if (consume("position"))  { position(); }
        else if (consume("stop"))      { stop(); }
        else if (consume("ponderhit")) { ponderhit(); }
        else if (consume("isready"))   { info_readyok(); }
        else if (consume("setoption")) { setoption(); }
        else if (consume("set"))       { setoption(); }
        else if (consume("ucinewgame")){ ucinewgame(); }
        else if (consume("uci"))       { uciok(); }
        else if (consume("debug"))     { setDebugOn(); }
        else if (consume("perft"))     { goPerft(); }
        else if (consume("bench"))     { bench(); }
        else if (consume("quit"))      { stop(); break; }
        else if (consume("exit"))      { stop(); break; }

        if (hasMoreInput()) {
            std::string unparsedInput;
            inputLine.clear();
            std::getline(inputLine, unparsedInput);
            error("unparsed input->" + unparsedInput);
        }
    }
}

void Uci::uciok() const {
    Output ob{this};
    ob << "id name " << io::app_version << '\n';
    ob << "id author Aleks Peshkov\n";
    ob << "option name Debug Log File type string default " << (logFileName.empty() ? "<empty>" : logFileName) << '\n';
    ob << "option name EvalFile type string default " << (evalFileName.empty() ? "<empty>" : evalFileName) << '\n';
    ob << "option name Hash type spin"
       << " min "     << ::mebi(tt.minSize())
       << " max "     << ::mebi(tt.maxSize())
       << " default " << ::mebi(tt.size())
       << '\n';
    limits.uciok(ob);
    ob << "option name UCI_Chess960 type check default " << (chessVariant().is(Chess960) ? "true" : "false") << '\n';
    ob << "uciok";
}

void Uci::setoption() {
    consume("name");

    if (consume("Debug Log File")) {
        consume("value");

        inputLine >> std::ws;
        std::string newFileName;
        std::getline(inputLine, newFileName);
        ::rtrim(newFileName);

        if (newFileName.empty() || newFileName == "<empty>") {
            if (logFile.is_open()) {
                info("Debug Log File set <empty>");
                logFile.close();
            }
            logFileName.clear();
            return;
        }

        if (newFileName != logFileName) {
            if (logFile.is_open()) {
                info("Debug Log File set \"" + newFileName + "\"");
                logFile.close();
                logFileName.clear();
            }

            logFile.open(newFileName, std::ios::app);
            if (!logFile.is_open()) {
                error("failed opening Debug Log File: " + newFileName);
                return;
            }
            logFileName = std::move(newFileName);
            logStartTime = ::timeNow();
        }

        return;
    }

    if (consume("EvalFile")) {
        consume("value");

        inputLine >> std::ws;
        std::string newFileName;
        std::getline(inputLine, newFileName);
        ::rtrim(newFileName);

        if (newFileName.empty() || newFileName == "<empty>") {
            info("EvalFile set <empty>");
            setEmbeddedEval();
            return;
        }

        loadEvalFile(newFileName);
        return;
    }

    if (consume("Hash")) {
        consume("value");
        setHash();
        return;
    }

    if (consume("UCI_Chess960")) {
        consume("value");

        if (consume("true"))  { chessVariant_ = ChessVariant{Chess960}; return; }
        if (consume("false")) { chessVariant_ = ChessVariant{Orthodox}; return; }

        io::fail_rewind(inputLine);
        return;
    }

    limits.setoption(inputLine);
}

void Uci::setDebugOn() {
    if (!hasMoreInput()) {
        Output ob{this};
        ob << "info string debug is " << (debugOn_ ? "on" : "off");
        return;
    }

    if (consume("on"))  { debugOn_ = true; info("debug on"); return; }
    if (consume("off")) { debugOn_ = false; info("debug off"); return; }

    io::fail_rewind(inputLine);
}

void Uci::setEmbeddedEval() {
    nnue.setEmbeddedEval();
    evalFileName.clear();
    ucinewgame();
}

void Uci::loadEvalFile(const std::string& fileName) {
    std::ifstream file(fileName, std::ios::binary);

    if (!file.is_open()) {
        error("failed opening EvalFile " + fileName);
        return;
    }

    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(std::ios::beg);

    if (fileSize == std::streamsize(-1)) {
        error("failed reading size of EvalFile " + fileName);
        return;
    }

    if (static_cast<size_t>(fileSize) != sizeof(nnue)) {
        error("EvalFile size mismatch, expected " + std::to_string(sizeof(nnue)) + ", file size " + std::to_string(fileSize));
        return;
    }

    std::vector<char> buffer(sizeof(nnue));
    file.read(buffer.data(), sizeof(nnue));
    if (!file) {
        error("failed reading EvalFile " + fileName);
        return;
    }

    // everything is ok
    std::memcpy(&nnue, buffer.data(), sizeof(nnue));
    evalFileName = std::move(fileName);

    info("loaded EvalFile " + fileName);

    // reset accumulator bias state
    ucinewgame();
    return;
}

void Uci::setHash() {
    size_t quantity = 0;
    inputLine >> quantity;
    if (!inputLine) {
        io::fail_rewind(inputLine);
        return;
    }

    io::char_type unit{'m'};
    inputLine >> unit;

    switch (std::tolower(unit)) {
        case 't':
            quantity *= 1024;
            /* fallthrough */
        case 'g':
            quantity *= 1024;
            /* fallthrough */
        case 'm':
            quantity *= 1024;
            /* fallthrough */
        case 'k':
            quantity *= 1024;
            /* fallthrough */
        case 'b':
            break;

        default: {
            io::fail_rewind(inputLine);
            return;
        }
    }

    tt.setSize(quantity);
    newGame();
}

void Uci::ucinewgame() {
    newGame();
    position_.setStartpos(); //TRICK: also counts root moves
    repetitions.clear();
    repetitions.push(colorToMove(), position_.z());
}

void Uci::position() {
    if (!hasMoreInput()) {
        UciOutput ob{this};
        ::info_fen(ob, position_);
        return;
    }

    mainSearchThread.waitReady();

#ifndef NDEBUG
    debugPosition = inputLine.str();
#endif

    if (consume("startpos")) {
        position_.setStartpos();
        repetitions.clear();
        repetitions.push(colorToMove(), position_.z());
    }

    if (consume("fen")) {
        position_.readFen(inputLine);
        repetitions.clear();
        repetitions.push(colorToMove(), position_.z());
    }

    consume("moves");
    position_.playMoves(inputLine, repetitions);
    tt.prefetch<TtSlot>(position_.z());
    position_.countRootMoves();
}

void Uci::go() {
    newSearch();

#ifndef NDEBUG
    debugGo = inputLine.str();
#endif

    limits.go(inputLine, position_.sideOf(White), &position_);
    if (consume("searchmoves")) { position_.limitMoves(inputLine); }

    if (position_.rootMoves() <= 1) {
        // fast return on 0 or 1 root legal moves
        pv.set(position_.rootMoves() == 0 ? HistoryMove{} : position_.firstRootMove());
        info_bestmove();
        return;
    }

    auto started = mainSearchThread.start([this] {
        Node{position_, *this}.searchRoot();
        info_bestmove();
    });
    if (started) {
        std::this_thread::yield();
        return;
    }

    if (bestmove_.empty()) {
        error("search not started, send bestmove 0000");
        pv.set({});
        info_bestmove();
    } else {
        error("search not started, bestmove not empty:" + bestmove_);
        sendDelayedBestMove();
    }
}

void Uci::sendDelayedBestMove() const {
    std::string bestmove;
    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        bestmove = std::exchange(bestmove_, "");
    }

    if (!bestmove.empty()) {
        if (debugOn_) { info("sending delayed bestmove: " + bestmove); }
        output(bestmove); // usually empty
    }
}

void Uci::stop() {
    sendDelayedBestMove();
    limits.stop();
    std::this_thread::yield();
}

void Uci::ponderhit() {
    sendDelayedBestMove();
    limits.ponderhit();
    std::this_thread::yield();
}

void Uci::info_pv() const {
#ifdef NDEBUG
    bool flush = limits.getNodes() >= 1'000'000;
#else
    bool flush = true;
#endif

    UciOutput ob{this, flush};
    ::info_pv(ob, pv, limits);
}

void Uci::info_bestmove() const {
    UciOutput ob{this};
    auto delayed = limits.shouldDelayBestmove();

    if (limits.hasNewNodes()) {
        ::info_pv(ob, pv, limits);
        if (delayed) { ob.flush(); } else { ob << '\n'; }
    }

    ob << "bestmove" << pv.move(0_ply);
    if (limits.canPonder() && pv.move(1_ply).any()) {
        ob << " ponder" << pv.move(1_ply);
    }

    if (delayed) {
        {
            std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
            if (!bestmove_.empty()) { info("old bestmove ignored: " + bestmove_); }
            bestmove_ = std::move(ob).str();
            // will be sent later by 'stop' or 'ponderhit'
        }
        ob.str("");
        if (debugOn_) { info("bestmove output delayed: " + bestmove_); }
    }
}

void Uci::info_readyok() const {
    UciOutput ob{this};
#ifndef NDEBUG
    ::info_fen(ob, position_) << '\n';
#endif
    if (limits.hasNewNodes() && limits.getNodes() > 0) { ::info_pv(ob, pv, limits) << '\n'; }
    ob << "readyok";
}

void Uci::goPerft() {
    newSearch();

    Ply depth{1};
    inputLine >> depth;
    depth = std::min<Ply>(depth, 18_ply); // current Tt implementation limit

    mainSearchThread.start([this, depth] {
        NodePerft{position_, *this, depth}.visitRoot();
        info_perft_bestmove();
    } );
}

void Uci::info_perft_bestmove() const {
    Output ob{this};
    if (limits.hasNewNodes()) {
        ob << "info"; limits.nps(ob) << '\n';
    }
    ob << "bestmove 0000";
}

void Uci::info_perft_depth(Ply depth, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << depth << " perft " << perft;
}

void Uci::info_perft_currmove(int moveCount, HistoryMove currentMove, node_count_t perft) const {
    UciOutput ob{this};
    ob << "info currmovenumber " << moveCount;
    limits.nps(ob);
    ob << " currmove" << currentMove;
    ob << " perft " << perft;
}

void Uci::bench() {
    std::string goLimits;

    inputLine >> std::ws;
    std::getline(inputLine, goLimits);

    bench(goLimits);
}

void Uci::bench(std::string& goLimits) {
    if (goLimits.empty()) {
#ifndef NDEBUG
        goLimits = "depth 9 nodes 100000"; // default for slow debug build
#else
        goLimits = "depth 18 nodes 50000000"; // default
#endif
    }

    std::istringstream is{goLimits};

    static std::string_view positions[][2] = {
        //{"2r2rk1/ppR5/1n1n4/3PNP2/3q3p/5Qp1/P5PP/1B3R1K w - - 0 28", "bm c7g7; id mate#11 talkchess.com/forum/viewtopic.php?p=937997"},
        {"1B1Q2K1/q1p4P/4P3/3Pk1p1/1r1NrR1b/4pn1P/1pRp2n1/1B2N2b w - -", "bm c2c7; id mate#2 talkchess.com/viewtopic.php?p=190985"},
        {"3R1R2/K3k3/1p1nPb2/pN2P2N/nP1ppp2/4P3/6P1/4Qq1r w - -", "bm e1e2; id mate#5 talkchess.com/viewtopic.php?p=904264"}, // depth 13
        {"8/1Pp5/nP5K/p7/8/8/PR6/2r4k w - -", "bm b7b8n id Dann Corbit aleks.underpromotion.09"},
        {"1k2b3/4bpp1/p2pp1P1/1p3P2/2q1P3/4B3/PPPQN2r/1K1R4 w - -", "bm f5f6 id Dann Corbit aleks.pawn-race.01"},
        {"2b3r1/6pp/1kn2p2/7N/ppp1PN2/5P2/1PP2KPP/R7 b - - 1 28", "bm b6a5 talkchess.com/forum/viewtopic.php?t=85672"},
        {"2kr3r/Qbp1q1bp/1np3p1/5p2/2P1pP2/1PN3P1/PBK3BP/3RR3 w - - 0 21", "bm e1e4; id petrel 20251206"},
        {"6k1/p1rqbppp/1p2p3/nb1pP3/3P1NBP/PP4P1/5PN1/R2Q2K1 w - - 0 26", "bm f4e6; id petrel 20251231"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "id startpos"},
    };

    uciok();

    node_count_t benchNodes{0};
    auto benchStart{::timeNow()};
    for (auto pos : positions) {
        std::string fen{pos[0]};
        inputLine.clear();
        inputLine.str(fen);

        position_.readFen(inputLine);
        if (hasMoreInput()) {
            error("failed parsing bench fen: " + fen);
            continue;
        }

        {
            UciOutput ob{this};
            ob << "\n"; ::info_fen(ob, position_); ob << " ; " << pos[1];
            ob << "\ngo " << goLimits;
        }

        is.clear();
        is.seekg(0);
        newGame();
        newSearch();

        limits.go(is, position_.sideOf(White));

        Node{position_, *this}.searchRoot();
        benchNodes += limits.getNodes();
        info_bestmove();
    }
    auto benchTime{::elapsedSince(benchStart)};

    {
        Output ob{this};
        ob << '\n'
            << Sep{tt.writes} << " tt-writes, "
            << Sep{tt.hits} << " tt-hits, "
            << Sep{tt.reads} << " tt-reads\n"
            << Sep{benchNodes} << " nodes "
            << Sep{::nps(benchNodes, benchTime)} << " nps";
    }
}
