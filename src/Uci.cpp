#include <set>
#include "perft.hpp"
#include "search.hpp"
#include "System.hpp"
#include "Uci.hpp"
#include "Position_impl.hpp"

namespace io {

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

} // namespace io

namespace { // anonymous namespace

istream& operator >> (istream& is, TimeInterval& timeInterval) {
    int msecs;
    if (is >> msecs) {
        if (msecs < 0) { msecs = 0; }
        timeInterval = std::chrono::duration_cast<TimeInterval>(std::chrono::milliseconds{msecs} );
    }
    return is;
}

ostream& operator << (ostream& os, const TimeInterval& timeInterval) {
    return os << std::chrono::duration_cast<std::chrono::milliseconds>(timeInterval).count();
}

template <typename nodes_type, typename duration_type>
constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
    return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
}

static constexpr size_t mebibyte = 1024 * 1024;

template <typename T> static T mebi(T bytes) { return bytes / mebibyte; }
template <typename T> static constexpr T permil(T n, T m) { return (n * 1000) / m; }

template <typename T> concept Formattable = requires(const T& t, ostream& os) { t.format(os); };
template <Formattable F> ostream& operator<<(ostream& os, const F& formattable) { formattable.format(os); return os; }

// apos ' after millions
struct Mega {
    u64_t v;
    ostream& format(ostream& os) const {
        if (v == 0) return os << '0';

        char buf[28];
        int idx = 27;
        buf[idx--] = '\0';

        int digitCount = 0;
        u64_t n = v;
        do {
            if (digitCount != 0 && digitCount % 6 == 0) {
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
class Output : public std::ostringstream {
protected:
    const Uci& uci;
    bool flush_;
public:
    Output (const Uci* _uci, bool flush = true) : std::ostringstream{}, uci{*_uci}, flush_{flush} {}
    std::ostringstream& flush(bool _flush = true) { uci.output(view(), _flush); str({}); return *this; }
    ~Output () { flush(flush_); }
};

// std::ostringstream UciMove capable output buffer, flushed on destruction
class UciOutput : public Output {
    Color colorToMove;
public:
    UciOutput(const Uci* _uci, bool flush = true) : Output{_uci, flush}, colorToMove{uci.colorToMove()} {}
    ChessVariant chessVariant() const { return uci.chessVariant(); }
    Color color() const { return colorToMove; }
    Color flipColor() { colorToMove = ~colorToMove; return colorToMove; }
    void resetRootColor() { colorToMove = uci.colorToMove(); }
};

// typesafe operator<< chaining
UciOutput& operator << (UciOutput& ob, io::czstring message) {
    static_cast<ostream&>(ob) << message; return ob;
}

// convert move to UCI format
UciOutput& operator << (UciOutput& ob, Move move) {
    bool isWhite{ob.color().is(White)};
    ob.flipColor();
    ob << ' ';

    if (move.none()) {
        ob << "0000";
        return ob;
    }

    Square from{move.from()};
    Square to{move.to()};

    Square uciFrom{isWhite ? from : ~from};
    Square uciTo{isWhite ? to : ~to};

    if (!move.isSpecial()) {
        ob << uciFrom << uciTo;
        return ob;
    }

    // pawn promotion
    if (from.on(Rank7)) {
        // the type of a promoted pawn piece encoded in place of move to's rank
        uciTo = Square{to.file(), isWhite ? Rank8 : Rank1};
        ob << uciFrom << uciTo << PromoType{::promoTypeFrom(to.rank())};
        return ob;
    }

    // en passant capture
    if (from.on(Rank5) && to.on(Rank5)) {
        // en passant capture move internally encoded as pawn captures pawn
        ob << uciFrom << Square{to.file(), isWhite ? Rank6 : Rank3};
        return ob;
    }

    //TRICK: castling is "pawn" move as it needs special handling
    if (from.on(Rank1)) {
        // castling move internally encoded as the rook captures own king

        if (ob.chessVariant().is(Orthodox)) {
            if (from.on(FileA)) { ob << uciTo << Square{File{FileC}, uciFrom.rank()}; return ob; }
            if (from.on(FileH)) { ob << uciTo << Square{File{FileG}, uciFrom.rank()}; return ob; }
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
    auto* pvMoves = pv.moves();
    if (pvMoves->none()) { return ob; } // empty PV (no legal moves at root)

    {
        ob << " pv";
        for (Move move; (move = *pvMoves++).any(); ) {
            ob << move;
        }
        ob.resetRootColor();
    }
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
            if (sq1.rank() != sq2.rank()) {
                return sq1.rank() < sq2.rank(); // Rank8 < Rank1
            }
            else {
                // FileD > FileE > FileC > FileF > FileB > FileG > FileA > FileH
                // order gains a few Elo
                constexpr array<int, File> order{6, 4, 2, 0, 1, 3, 5, 7};
                return order[sq1.file()] < order[sq2.file()];
            }
        }
    };
    using Squares = std::set<Square, SquareImportance>;

    array<Squares, Color, PieceType> pieces;
    array<int, Color> pieceCount = {{0, 0}};

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
            auto f = +file + (c - '1');
            if (!File::isOk(f)) {
                return io::fail_char(is);
            }

            //avoid out of range initialization check
            file = File{static_cast<File::_t>(f)};
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
    if (pieceCount[color] == Pi::size()) {
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

            switch (*chessVariant) {
                case Chess960:
                    castlingSymbol = positionSide.sq(pi).file().to_char();
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

UciOutput& operator << (UciOutput& ob, const UciPosition& pos) {
    fen(ob, ob.chessVariant(), pos, ob.color(), pos.fullMoveNumber());
    return ob;
}

UciOutput& info_fen(UciOutput& ob, const UciPosition& pos) {
    ob << "info" << pos.evaluate();
    ob << " fen " << pos;
    return ob;
}

} // anonymous namespace

istream& UciPosition::readMove(istream& is, Square& from, Square& to) const {
    auto before = is.tellg();
    is >> from >> to;
    if (!is) { return io::fail_pos(is, before); }

    if (colorToMove_.is(Black)) { from = ~from; to = ~to; }

    if (!MY.has(from)) { return io::fail_pos(is, before); }

    //convert special moves (castling, promotion, ep) to the internal move format
    if (MY.isPawn(from)) {
        if (from.on(Rank7)) {
            PromoType promo{Queen};
            is >> promo;
            is.clear(); //promotion piece is optional
            to = Square{to.file(), ::rankOf(promo)};
            return is;
        }

        if (from.on(Rank5) && OP.hasEnPassant() && OP.fileEnPassant().is(to.file())) {
            to = Square{to.file(), Rank5};
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

void UciPosition::playMoves(istream& is, Repetitions& repetitions) {
    while (is >> std::ws && !is.eof()) {
        auto before = is.tellg();

        Square from; Square to;

        if (!readMove(is, from, to) || !isPossibleMove(from, to)) {
            io::fail_pos(is, before);
            return;
        }

        Position::makeMove(from, to);
        generateMoves();
        colorToMove_ = ~colorToMove_;

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
            Side side = sideOf(*color);

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
        if (!ep.on(colorToMove_.is(White) ? Rank6 : Rank3) || !setEnPassant(ep.file())) {
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

// fast exit: return the first legal move found
Move UciPosition::firstRootMove() const {
    for (Pi pi : MY.any()) {
        if (bbMovesOf(pi).none()) { continue; }
        return toMove(MY.sq(pi), bbMovesOf(pi).first());
    }
    return {};
}

TimePoint SearchLimits::newSearch() {
    stop_.store(false, std::memory_order_release);
    nodes_ = 0;
    quotaCounter_ = 0;
    quotaLimit_ = QuotaLimit;
    lastMove_ = {};
    searchStartTime_ = timeNow();
    return searchStartTime_;
}

void SearchLimits::stop() {
    pondering_.store(false, std::memory_order_relaxed);
    stop_.store(true, std::memory_order_release);
}

void SearchLimits::ponderhit() {
    pondering_.store(false, std::memory_order_relaxed);
}

bool SearchLimits::setLimits(const UciLimits& go, const UciPosition& position) {
    const Color my{ position.colorToMove() };

    maxDepth_ = go.depth;
    nodesLimit_ = go.nodes;
    pondering_.store(go.ponder, std::memory_order_relaxed);

    // minimum reasonable thinking time (search 100 nodes)
    auto setMinimumThinkingTime = [&]() { timePool_ = 0ms; timeStrategy_ = ExactTime; quotaLimit_ = QuotaLimitSmall; };

    if (go.nodes <= 0) { setMinimumThinkingTime(); return false; }

    if (go.movetime > 0ms) {
        timePool_ = go.movetime;
        timeStrategy_ = ExactTime;
        return true;
    }

    auto noTimeGiven = go.time[my] <= 0ms && go.inc[my] <= 0ms; // `go` without time limits
    if (go.infinite || noTimeGiven) {
        timePool_ = UnlimitedTime;
        timeStrategy_ = ExactTime;
        return true;
    }

    //TRICK: some GUI can send negative `time` when soft clock used, use `inc` for available time
    auto availableTime = go.time[my] <= 0ms ? go.time[my] + go.inc[my] : go.time[my];

    availableTime -= go.moveOverhead;
    if (availableTime <= 0ms) { io::error("availableTime <= 0ms"); setMinimumThinkingTime(); return true; }

    // [0..6] startpos = 6, queens exchanged = 4, R vs R endgame = 1
    int gamePhase = position.gamePhase();
    lowMaterialQuotaBonus_ = 4 - std::clamp(gamePhase, 1, 5);

    const auto lookAheadMoves = 0 < go.movestogo && go.movestogo < LookAheadMoves ? go.movestogo : LookAheadMoves;
    const auto lookAheadTime = [&](Color color) { return go.time[color] + go.inc[color] * (lookAheadMoves - 1); };
    const auto averageMoveTime = [&](Color color) {
        return lookAheadTime(color) / (lookAheadMoves < LookAheadMoves ? lookAheadMoves : LookAheadMoves);
    };

    // "maximum" time strategy: allocate 1/4 of all remaining time (including look ahead number of future time increments)
    auto maximumTime = lookAheadTime(my) / 4; // 25%
    {
        if (lookAheadMoves < LookAheadMoves) {
            // solved for f(1) = 100%, f(2) = 75%, f(16) = 25%
            maximumTime *= 19 + lookAheadMoves;
            maximumTime /= 3 + 2 * lookAheadMoves;
        }

        // left board material correction
        maximumTime *= 8 - std::clamp(gamePhase, 3, 5);
        maximumTime /= 4; // 75%, 100%, 125%

        maximumTime -= go.moveOverhead;
        if (maximumTime <= 0ms) { io::error("maximumTime <= 0ms"); setMinimumThinkingTime(); return true; }

        maximumTime = std::min(maximumTime, availableTime);
    }

    // "optimum" time strategy: allocate time evenly between look ahead number of moves
    auto optimumTime = averageMoveTime(my);
    {
        if (go.ponder) {
            auto ponderTime = std::min(averageMoveTime(~my), averageMoveTime(my)); // do not fully trust opponent time
            optimumTime += 5 * ponderTime / 8;
        }

        // allocate 1.6x more time for the first out of book move in the game (fill up TT and history data)
        if (go.isNewGame) { optimumTime *= 13; optimumTime /= 8; }

        // MaxQuota and/or HardMove time strategy may spend up to 5.12 times over average (optimium) move quota
        optimumTime *= 41 * MaxQuota * HardMove; // time * 512
        optimumTime /= 4096; //TRICK: OptimumTimeQuota * NormalMove = 100 ~= 4096/41

        optimumTime -= go.moveOverhead;
        if (optimumTime <= 0ms) { io::error("optimumTime <= 0ms"); setMinimumThinkingTime(); return true; }
    }

    timeStrategy_ = EasyMove;
    timePool_ = std::min(optimumTime, maximumTime);
    assert (timePool_ > 0ms);

    if (timePool_ < 1ms) { quotaLimit_ = QuotaLimitSmall; }
    return true;
}

Uci::Uci(ostream& os) :
    inputLine{std::string(2048, '\0')}, // preallocate 2048 bytes (~200 full moves)
    out_{os},
    bestmove_(sizeof("bestmove a7a8q ponder h2h1q"), '\0'),
    logStartTime{::timeNow()},
    pid_{System::getPid()},
    tt(64 * mebibyte)
{
    for (auto ply : range<Ply>()) { std::construct_at(&searchStack[ply], ply); }
    inputLine.clear();
    bestmove_.clear();
    setEmbeddedEval();
}

void Uci::newGame() {
    tt.newGame();
    contMoves.clear();
    counterCheck.clear();
    go_.isNewGame = true;
}

void Uci::newSearch() {
    std::string bestmove; // empty
    swapBestMove(bestmove); // cleanup
    if (!bestmove.empty()) { error("newsearch(), bestmove was not empty: " + bestmove); }

    lastInfoTime_ = limits.newSearch();
    lastInfoNodes_ = 0;
    tt.newSearch();
    rootBestMoves = {};
}

void Uci::output(std::string_view message, bool flush) const {
    if (message.empty()) { return; }

    {
        std::scoped_lock lock{outMutex};
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
        std::scoped_lock lock{outMutex};
        _log(tag, message);
    }
}

void Uci::error(std::string_view message) const {
    UciOutput ob{this};
    ob << "petrel " << pid_ << " " << message << '\n';

    // search debugging info
    if (limits.getNodes() > 0) {
        ob << "root fen " << position_;
        ob << " node " << limits.getNodes() << " depth " << pv.depth();
        ob << pv << '\n';

#ifndef NDEBUG
    if (!debugPosition.empty()) { ob << debugPosition << '\n'; }
    if (!debugGo.empty()) { ob << debugGo << '\n'; }
#endif

    }

    std::cerr << ob.str() << std::flush;
    log('!', ob.str());

    ob.str({});
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
        else if (consume("perft"))     { perft(); }
        else if (consume("bench"))     { bench(); }
        else if (consume("wait"))      { wait(); }
        else if (consume("quit"))      { break; }
        else if (consume("exit"))      { break; }

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
    ob << "id name " << io::app_version;
    ob << "\nid author Aleks Peshkov";
    ob << "\noption name Debug Log File type string default " << (logFileName.empty() ? "<empty>" : logFileName);
    ob << "\noption name EvalFile type string default " << (evalFileName.empty() ? "<empty>" : evalFileName);
    ob << "\noption name Hash type spin min " << ::mebi(tt.minSize()) << " max " << ::mebi(tt.maxSize()) << " default " << ::mebi(tt.size());
    ob << "\noption name Move Overhead type spin min " << UciLimits::MoveOverheadDefault << " max 10000 default " << go_.moveOverhead;
    ob << "\noption name Ponder type check default " << (go_.canPonder ? "true" : "false");
    ob << "\noption name UCI_Chess960 type check default " << (chessVariant().is(Chess960) ? "true" : "false");
    ob << "\nuciok";
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

    if (consume("Move Overhead")) {
        consume("value");

        TimeInterval moveOverhead{0};
        inputLine >> moveOverhead;
        go_.moveOverhead = std::max(go_.moveOverhead, UciLimits::MoveOverheadDefault);

        if (!inputLine) { io::fail_rewind(inputLine); }
        return;
    }

    if (consume("Ponder")) {
        consume("value");

        if (consume("true"))  { go_.canPonder = true; return; }
        if (consume("false")) { go_.canPonder = false; return; }

        io::fail_rewind(inputLine);
        return;
    }
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
    readStartPos();
    setPositionMoves();
}

void Uci::readStartPos() {
    static std::istringstream startpos{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    position_.readFen(startpos);

    // rewind for next use:
    startpos.clear();
    startpos.seekg(0);
}

void Uci::position() {
    if (!hasMoreInput()) {
        UciOutput ob{this};
        ::info_fen(ob, position_);
        return;
    }

    wait();

#ifndef NDEBUG
    debugPosition = inputLine.str();
#endif

    if (consume("fen")) {
        position_.readFen(inputLine);
    } else if (consume("startpos")) {
        readStartPos();
    }

    setPositionMoves();
}

void Uci::setPositionMoves() {
    repetitions.clear();
    repetitions.push(colorToMove(), position_.z());

    if (consume("moves")) {
        position_.playMoves(inputLine, repetitions);
    }

    do {
        auto ttSlot = *tt.addr<TtSlot>(position_.z());
        if (ttSlot != position_.z()) { break; }

        auto ttMove = ttSlot.ttMove();
        if (ttMove.none()) { break; }
        if (!position_.isPossibleMove(ttMove.from(), ttMove.to())) { break; }

        Score score{NoScore};
        if (ttSlot.bound() == ExactScore) {
            score = ttSlot.score(0_ply);
        }

        pv.set(position_.toMove(ttMove), score);
        return;
    } while (false);

    // TT miss
    pv.set(position_.firstRootMove()); // some legal move in worst case
}

void Uci::go() {
    newSearch();

    do {
        #ifndef NDEBUG
            debugGo = inputLine.str();
        #endif

        if (position_.movesTotal() == 0) { break; } // nothing to search

        go_.readGo(inputLine);
        if (hasMoreInput()) { break; } // parsing error
        infinite_ = go_.infinite;

        if (position_.movesTotal() == 1) {
            // immediate move, not an error case, optimization
            //TODO: add ponder move
            info_bestmove();
            return;
        } else if (limits.setLimits(go_, position_)) {
            auto started = mainSearchThread.start([this] {
                searchStack[0_ply].searchRoot(position_);
                info_bestmove();
            });
            if (!started) { break; }

            go_.isNewGame = false;
            std::this_thread::yield();
            return;
        }
    } while (false);

    // error: search not started, report some bestmove without any search
    {
        UciOutput ob{this};
        ob << "bestmove" << pv.getMove(0_ply);
    }

    std::string bestmove; // empty
    swapBestMove(bestmove); // cleanup

    if (bestmove.empty()) {
        error("search not started");
    } else {
        error("search not started, bestmove was not empty: " + bestmove);
    }
}

void UciLimits::readGo(istream& is) {
    constexpr Color w{White};
    constexpr Color b{Black};

    time = {0ms, 0ms}; // `wtime`/`btime`
    inc = {0ms, 0ms}; // `winc`/`binc`
    movetime = {0ms};
    nodes = NodeCountMax;
    movestogo = 0;
    depth = MaxPly;
    ponder = false;
    infinite = false;

    while (is >> std::ws, !is.eof()) {
        if      (io::consume(is, "wtime"))    { is >> time[w]; }
        else if (io::consume(is, "btime"))    { is >> time[b]; }
        else if (io::consume(is, "winc"))     { is >> inc[w]; }
        else if (io::consume(is, "binc"))     { is >> inc[b]; }
        else if (io::consume(is, "movetime")) { is >> movetime; }
        else if (io::consume(is, "nodes"))    { is >> nodes; }
        else if (io::consume(is, "movestogo")){ is >> movestogo; if (movestogo < 0) { movestogo = 0; } }
        else if (io::consume(is, "depth"))    { int d; is >> d;  if (Ply::isOk(d)) { depth = Ply{d}; } }
        else if (io::consume(is, "ponder"))   { ponder = true; }
        else if (io::consume(is, "infinite")) { infinite = true; }
        else { break; }
    }
}

void Uci::outputBestMove() {
    std::string bestmove; // empty
    swapBestMove(bestmove); // cleanup
    output(bestmove); // usually empty and does nothing
}

void Uci::swapBestMove(std::string& bestmove) {
    std::scoped_lock lock{bestmoveMutex};
    bestmove_.swap(bestmove);
}

void Uci::stop() {
    infinite_ = false;
    outputBestMove();
    limits.stop();
    std::this_thread::yield();
}

void Uci::ponderhit() {
    outputBestMove();
    limits.ponderhit();
    std::this_thread::yield();
}

void Uci::wait() {
    mainSearchThread.waitNotBusy();
}

ostream& Uci::average_nps(ostream& os) const {
    auto nodes = limits.getNodes();
    auto time = ::timeNow();
    auto elapsedTime = time - limits.searchStartTime();

    os << " nodes " << nodes;
    if (elapsedTime >= 1ms) {
        os << " time " << elapsedTime << " nps " << ::nps(nodes, elapsedTime);
    }

    lastInfoNodes_ = nodes;
    lastInfoTime_ = time;
    return os;
}

ostream& Uci::instant_nps(ostream& os) const {
    auto nodes = limits.getNodes();
    auto time = ::timeNow();
    auto elapsedTime = time - limits.searchStartTime();

    auto deltaNodes = nodes - lastInfoNodes_;
    auto deltaTime = time - lastInfoTime_;

    os << " nodes " << nodes;
    if (elapsedTime >= 1ms) {
        os << " time " << elapsedTime << " nps " << ::nps(deltaNodes, deltaTime);
    }

    lastInfoNodes_ = nodes;
    lastInfoTime_ = time;
    return os;
}

void Uci::info_pv() const {
#ifdef NDEBUG
    bool flush = limits.getNodes() >= 1'000'000;
#else
    bool flush = true;
#endif

    UciOutput ob{this, flush};
    ob << "info depth " << pv.depth(); instant_nps(ob); ob << pv;
}

void Uci::info_bestmove() {
    UciOutput ob{this};
    auto delayed = limits.pondering() || infinite_;

    if (hasNewNodes()) {
        ob << "info depth " << pv.depth(); average_nps(ob); ob << pv;
        if (delayed) { ob.flush(); } else { ob << '\n'; }
    }

    ob << "bestmove" << pv.getMove(0_ply);
    if (go_.canPonder && pv.getMove(1_ply).any()) {
        ob << " ponder" << pv.getMove(1_ply);
    }

    if (delayed) {
        std::string bestmove(ob.str());
        swapBestMove(bestmove); // save bestmove for later output
        if (!bestmove.empty()) { info("old bestmove ignored: " + bestmove); }
        ob.str({}); // prevent sending now
    }
}

void Uci::info_readyok() const {
    UciOutput ob{this};
    ob << "readyok";
    if (hasNewNodes()) {
        ob << "\ninfo depth " << pv.depth(); instant_nps(ob); ob << pv;
    }
}

void Uci::perft() {
    newSearch();

    Ply depth{1};
    inputLine >> depth;
    depth = std::min<Ply>(depth, 18_ply); // current Tt implementation limit

    mainSearchThread.start([this, depth] {
        NodePerft{position_, depth}.visitRoot();
        info_perft_bestmove();
    } );
}

void Uci::info_perft_bestmove() const {
    Output ob{this};
    if (hasNewNodes()) { ob << "info"; average_nps(ob) << '\n'; }
    ob << "bestmove 0000";
}

void Uci::info_perft_depth(Ply depth, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << depth; average_nps(ob) << " perft " << perft;
}

void Uci::info_perft_currmove(int moveCount, Move currentMove, node_count_t perft) const {
    UciOutput ob{this};
    ob << "info currmovenumber " << moveCount;
    instant_nps(ob);
    ob << " currmove" << currentMove;
    ob << " perft " << perft;
}

void Uci::bench() {
    std::string goLimits;

    inputLine >> std::ws;
    std::getline(inputLine, goLimits);

    bench(goLimits);
}

void Uci::bench(std::string_view goLimits) {
    if (goLimits.empty()) {
#ifndef NDEBUG
        goLimits = "depth 9 nodes 100000"; // default for slow debug build
#else
        goLimits = "depth 18 nodes 50000000"; // default
#endif
    }

    std::istringstream goStream{std::string{goLimits}};
    go_.readGo(goStream);

    static constexpr std::string_view positions[][2] = {
        //{"2r2rk1/ppR5/1n1n4/3PNP2/3q3p/5Qp1/P5PP/1B3R1K w - - 0 28", "bm c7g7; id mate#11 talkchess.com/forum/viewtopic.php?p=937997"},
        {"1B1Q2K1/q1p4P/4P3/3Pk1p1/1r1NrR1b/4pn1P/1pRp2n1/1B2N2b w - -", "bm c2c7; id mate#2 talkchess.com/viewtopic.php?p=190985"},
        {"3R1R2/K3k3/1p1nPb2/pN2P2N/nP1ppp2/4P3/6P1/4Qq1r w - -", "bm e1e2; id mate#5 talkchess.com/viewtopic.php?p=904264"}, // depth 13
        {"8/1Pp5/nP5K/p7/8/8/PR6/2r4k w - -", "bm b7b8n id Dann Corbit aleks.underpromotion.09"},
        {"1k2b3/4bpp1/p2pp1P1/1p3P2/2q1P3/4B3/PPPQN2r/1K1R4 w - -", "bm f5f6 id Dann Corbit aleks.pawn-race.01"},
        {"2b3r1/6pp/1kn2p2/7N/ppp1PN2/5P2/1PP2KPP/R7 b - - 1 28", "bm b6a5 talkchess.com/forum/viewtopic.php?t=85672"},
        {"2kr3r/Qbp1q1bp/1np3p1/5p2/2P1pP2/1PN3P1/PBK3BP/3RR3 w - - 0 21", "bm e1e4; id petrel 20251206"},
        {"2r3k1/p1rqbppp/1pn1p3/1b1pP3/3P1N1P/5NPB/PP3P2/R1RQ2K1 w - - 0 20 moves f3e1 c6b4 c1c7 c8c7 h3g4 b5a4 b2b3 a4b5 a2a3 b4c6 e1g2 c6a5", "bm f4e6; id petrel 20251231"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "id startpos"},
    };

    uciok();

    node_count_t benchNodes{0};
    TimeInterval benchTime{0};
    node_count_t ttHits{0};
    node_count_t ttReads{0};
    node_count_t ttWrites{0};

    for (auto pos : positions) {
        auto fen{pos[0]};
        inputLine.clear();
        inputLine.str({fen.data(), fen.size()});

        position_.readFen(inputLine);
        setPositionMoves();

        if (hasMoreInput()) {
            error("failed parsing bench position fen " + std::string(fen));
            continue;
        }

        {
            Output ob{this};
            ob << "\n# " << pos[1];
            ob << "\nposition fen " << fen;
            ob << "\ngo " << goLimits;
        }

        newGame();
        newSearch();
        if (limits.setLimits(go_, position_)) {
            auto searchStart = lastInfoTime_;
            searchStack[0_ply].searchRoot(position_);

            benchTime += ::elapsedSince(searchStart);
            benchNodes += limits.getNodes();
            ttHits += tt.hits;
            ttReads += tt.reads;
            ttWrites += tt.writes;
        }

        info_bestmove();
    }

    if (benchTime > 0ms) {
        auto benchMicroseconds{ static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::microseconds>(benchTime).count()) };

        Output ob{this};
        ob << '\n'
            << Mega{ttWrites} << " tt-writes, " << Mega{ttHits} << " tt-hits, " << Mega{ttReads} << " tt-reads\n"
            << Mega{benchNodes} << " nodes " << Mega{(benchMicroseconds)} << " usec " << Mega{::nps(benchNodes, benchTime)} << " nps";
    }
}
