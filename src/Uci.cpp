#include "perft.hpp"
#include "search.hpp"
#include "Uci.hpp"

void trimTrailingWhitespace(std::string& str) {
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

class Output : public io::ostringstream {
protected:
    const Uci& uci;
public:
    Output (const Uci* u) : io::ostringstream{}, uci{*u} {}
    ~Output () { uci.output(str()); }
};

class UciOutput : public Output {
    Color colorToMove;

public:
    UciOutput(const Uci* u) : Output(u), colorToMove(uci.colorToMove()) {}

    Color color() const { return colorToMove; }
    Color flipColor() { return Color{colorToMove.flip()}; }

    ChessVariant chessVariant() const { return uci.chessVariant(); }
};

// convert move to UCI format
UciOutput& operator << (UciOutput& out, UciMove move) {
    if (!move) { out << "0000"; return out; }

    bool isWhite{ out.color() == White };
    Square from_ = move.from();
    Square to_ = move.to();
    Square from = isWhite ? from_ : ~from_;
    Square to = isWhite ? to_ : ~to_;

    if (!move.isSpecial()) { out << from << to; return out; }

    //pawn promotion
    if (from_.on(Rank7)) {
        //the type of a promoted pawn piece encoded in place of move to's rank
        Square promotedTo{File{to}, isWhite ? Rank8 : Rank1};
        out << from << promotedTo << PromoType{::promoTypeFrom(Rank{to_})};
        return out;
    }

    //en passant capture
    if (from_.on(Rank5)) {
        //en passant capture move internally encoded as pawn captures pawn
        assert (to_.on(Rank5));
        out << from << Square{File{to}, isWhite ? Rank6 : Rank3};
        return out;
    }

    //castling
    if (from_.on(Rank1)) {
        //castling move internally encoded as the rook captures the king

        if (out.chessVariant() == Orthodox) {
            if (from.on(FileA)) { out << to << Square{FileC, Rank{from}}; return out; }
            if (from.on(FileH)) { out << to << Square{FileG, Rank{from}}; return out; }
        }

        // Chess960:
        out << to << from;
        return out;
    }

    //should never happen
    assert (false);
    io::log("#invalid move in UCI output");
    out << "0000";
    return out;
}

UciOutput& operator << (UciOutput& out, const UciMove pv[]) {
    for (UciMove move; (move = *pv++); ) {
        out << " "; out << move; out.flipColor();
    }
    return out;
}

UciOutput& operator << (UciOutput& out, const PvMoves& pvMoves) {
    return out << static_cast<const UciMove*>(pvMoves);
}

namespace {
    static constexpr size_t mebibyte = 1024 * 1024;

    template <typename T>
    static T mebi(T bytes) { return bytes / mebibyte; }

    template <typename T>
    static constexpr T permil(T n, T m) { return (n * 1000) / m; }
}

Uci::Uci(ostream &o) :
    tt(16 * mebibyte),
    inputLine{std::string(1024, '\0')}, // preallocate 1024 bytes (~100 full moves)
    out{o},
    bestmove_(32, '\0')
{
    inputLine.clear();
    bestmove_.clear();
    ucinewgame();
}

void Uci::output(const std::string& message) const {
    if (message.empty()) { return; }

    {
        std::lock_guard<decltype(outMutex)> lock{outMutex};
        out << message << std::endl;
    }

    if (isDebugOn) { log(message); }
}

void Uci::log(const std::string& message) const {
    if (!logFile.is_open()) { return; }

    {
        std::lock_guard<decltype(logMutex)> lock{logMutex};
        logFile << message << std::endl;

        // recover if the logFile is in a bad state
        if (!logFile) {
            logFile.clear();
            logFile.close();
            logFile.open(logFileName, std::ios::app);
            logFile << "#logFile recovered from a bad state" << std::endl;
        }
    }
}

void Uci::processInput(istream& in) {
    std::string currentLine(1024, '\0'); // preallocate 1024 bytes (~100 full moves)
    while (std::getline(in, currentLine)) {
        if (isDebugOn) { log('>' + currentLine); }

        inputLine.clear(); //clear error state from the previous line
        inputLine.str(currentLine);
        inputLine >> std::ws;

        if      (consume("go"))        { go(); }
        else if (consume("position"))  { position(); }
        else if (consume("stop"))      { stop(); }
        else if (consume("ponderhit")) { ponderhit(); }
        else if (consume("isready"))   { readyok(); }
        else if (consume("setoption")) { setoption(); }
        else if (consume("set"))       { setoption(); }
        else if (consume("ucinewgame")){ ucinewgame(); }
        else if (consume("uci"))       { uciok(); }
        else if (consume("debug"))     { debug(); }
        else if (consume("perft"))     { goPerft(); }
        else if (consume("bench"))     { bench(); }
        else if (consume("quit"))      { stop(); break; }
        else if (consume("exit"))      { stop(); break; }

        if (hasMoreInput()) {
            std::string unparsedInput;
            inputLine.clear();
            std::getline(inputLine, unparsedInput);

            log(currentLine + "\n#unparsed input->" + unparsedInput);
        }
    }
}

void Uci::uciok() const {
    Output ob{this};
    ob << "id name " << io::app_version << '\n';
    ob << "id author Aleks Peshkov\n";
    ob << "option name Debug Log File type string default " << (logFileName.empty() ? "<empty>" : logFileName) << '\n';
    ob << "option name Hash type spin"
       << " min "     << ::mebi(tt.minSize())
       << " max "     << ::mebi(tt.maxSize())
       << " default " << ::mebi(tt.size())
       << '\n';
    ob << "option name Move Overhead type spin min 0 max 10000 default " << limits.moveOverhead << '\n';
    ob << "option name Ponder type check default " << (limits.canPonder ? "true" : "false") << '\n';
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
        ::trimTrailingWhitespace(newFileName);

        if (newFileName.empty() || newFileName == "<empty>") {
            if (logFile.is_open()) {
                log("#Debug Log File set <empty>");
                logFile.close();
            }
            logFileName.clear();
            return;
        }

        if (newFileName != logFileName) {
            if (logFile.is_open()) {
                log("#Debug Log File set \"" + newFileName + "\"");
                logFile.close();
                logFileName.clear();
            }

            logFile.open(newFileName, std::ios::app);
            if (!logFile.is_open()) {
                log("#Failed to open log file: " + newFileName);
                return;
            }
            logFileName = std::move(newFileName);
        }

        return;
    }

    if (consume("Hash")) {
        consume("value");
        setHash();
        return;
    }


    if (consume("Move Overhead")) {
        consume("value");

        inputLine >> limits.moveOverhead;
        if (limits.moveOverhead == 0ms) { limits.moveOverhead = 100us; }

        if (!inputLine) { io::fail_rewind(inputLine); }
        return;
    }

    if (consume("Ponder")) {
        consume("value");

        if (consume("true"))  { limits.canPonder = true; return; }
        if (consume("false")) { limits.canPonder = false; return; }

        io::fail_rewind(inputLine);
        return;
    }

    if (consume("UCI_Chess960")) {
        consume("value");

        if (consume("true"))  { position_.setChessVariant(ChessVariant{Chess960}); return; }
        if (consume("false")) { position_.setChessVariant(ChessVariant{Orthodox}); return; }

        io::fail_rewind(inputLine);
        return;
    }
}

void Uci::debug() {
    if (!hasMoreInput()) {
        Output ob{this};
        ob << "info string debug is " << (isDebugOn ? "on" : "off");
    }

    if (consume("on"))  { isDebugOn = true; log("#debug on"); return; }
    if (consume("off")) { isDebugOn = false; log("#debug off"); return; }

    io::fail_rewind(inputLine);
    return;
}

void Uci::setHash() {
    size_t quantity = 0;
    inputLine >> quantity;
    if (!inputLine) {
        io::fail_rewind(inputLine);
        return;
    }

    io::char_type unit = 'm';
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

    setHash(quantity);
}

void Uci::ucinewgame() {
    newGame();
    position_.setStartpos();
}

void Uci::position() {
    if (!hasMoreInput()) {
        Output ob{this};
        info_fen(ob);
        return;
    }

    if (consume("startpos")) {
        position_.setStartpos();
        repetitions.clear();
        repetitions.push(colorToMove(), position_.zobrist());
    }

    if (consume("fen")) {
        position_.readFen(inputLine);
        repetitions.clear();
        repetitions.push(colorToMove(), position_.zobrist());
    }

    consume("moves");
    position_.playMoves(inputLine, repetitions);
}

istream& UciSearchLimits::go(istream& in, Side white) {
    clear();

    while (in >> std::ws, !in.eof()) {
        if      (io::consume(in, "depth"))    { in >> depth; }
        else if (io::consume(in, "nodes"))    { in >> nodesLimit; }
        else if (io::consume(in, "movetime")) { in >> movetime; }
        else if (io::consume(in, "wtime"))    { in >> time[white]; }
        else if (io::consume(in, "btime"))    { in >> time[~white]; }
        else if (io::consume(in, "winc"))     { in >> inc[white]; }
        else if (io::consume(in, "binc"))     { in >> inc[~white]; }
        else if (io::consume(in, "movestogo")){ in >> movestogo; }
        else if (io::consume(in, "mate"))     { in >> mate; } // TODO: implement mate in n moves
        else if (io::consume(in, "ponder"))   { ponder.store(true, std::memory_order_relaxed); }
        else if (io::consume(in, "infinite")) { infinite.store(true, std::memory_order_relaxed); }
        else { break; }
    }

    setSearchDeadline();
    return in;
}

void Uci::go() {
    limits.go(inputLine, position_.sideOf(White));
    if (consume("searchmoves")) { position_.limitMoves(inputLine); }

    mainSearchThread.start([this] {
        newSearch();
        Node{position_, *this}.searchRoot();
        bestmove();
    });
    std::this_thread::yield();
}

void Uci::bestmove() {
    {
        UciOutput ob{this};
        ob << "info"; nps(ob); ob << pvScore << " pv"; ob << pvMoves;
    }

    UciOutput ob{this};

    ob << "bestmove "; ob << pvMoves[0];
    if (limits.canPonder && pvMoves[1]) {
        ob << " ponder "; ob.flipColor(); ob << pvMoves[1];
    }

    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (limits.shouldDelayBestmove()) {
            bestmove_  = std::string{ob.str()};
            ob.str("");
            return;
        }
    }

    limits.clear();
}

void Uci::stop() {
    limits.stop();
    std::this_thread::yield();

    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (!bestmove_.empty()) {
            output(bestmove_);
            bestmove_.clear();
            limits.clear();
            return;
        }
    }
}

void Uci::ponderhit() {
    limits.ponderhit();
    std::this_thread::yield();

    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (!bestmove_.empty()) {
            output(bestmove_);
            bestmove_.clear();
            limits.clear();
            return;
        }
    }
}

void Uci::goPerft() {
    Ply depth;
    inputLine >> depth;
    depth = std::min<Ply>(depth, 18); // current Tt implementation limit

    limits.clear();
    mainSearchThread.start([this, depth] {
        NodePerft{position_, *this, depth}.visitRoot();
        info_perft_bestmove();
    } );
}

void Uci::bench() {
    std::string goLimits;

    inputLine >> std::ws;
    std::getline(inputLine, goLimits);

    bench(goLimits);
}

void Uci::bench(std::string& goLimits) {
    if (goLimits.empty()) {
#ifdef NDEBUG
        goLimits = "depth 16 movetime 10000"; // default
#else
        goLimits = "depth 7 movetime 10000"; // default for slow build
#endif
    }

    static std::string positions[][2] = {
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 25", "id kiwipete"},
        {"1q1k4/2Rr4/8/2Q3K1/8/8/8/8 w - - 0 1", "bm g5h6; id zugzwang.002"},
        {"1k2r2r/pbb2p2/2qn2p1/8/PP6/2P2N2/1Q2NPB1/R4RK1 b - -", "bm c6f3; id mate # 7 CCC-I No.10"},
        {"6k1/p1rqbppp/1p2p3/nb1pP3/3P1NBP/PP4P1/5PN1/R2Q2K1 w - - 0 26", "bm f4e3; id petrel 20251231"},
        {"2kr3r/Qbp1q1bp/1np3p1/5p2/2P1pP2/1PN3P1/PBK3BP/3RR3 w - - 0 21", "bm e1e4; id petrel 20251206"},
        {"8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -", "bm a1b1; id Fine # 70"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "id startpos"},
    };

    uciok();

    node_count_t totalBenchNodes = 0;
    TimeInterval totalBenchTime = 0ms;

    for (auto pos : positions) {
        inputLine.clear();
        inputLine.str(pos[0]);
        position_.readFen(inputLine);
        if (hasMoreInput()) {
            log("error parsing bench fen: " + pos[0]);
            continue;
        }

        {
            Output ob{this};
            ob << "\n";
            info_fen(ob);
            ob << " ; " << pos[1];
            ob << "\ngo " << goLimits;
        }

        newGame();

        std::istringstream is{goLimits};
        limits.go(is, position_.sideOf(White));

        auto goStart = ::timeNow();
        Node{position_, *this}.searchRoot();
        totalBenchNodes += limits.getNodes();
        bestmove();
        totalBenchTime += elapsedSince(goStart);
    }

    Output ob{this};
    ob << "\n" << totalBenchNodes << " nodes " << ::nps(totalBenchNodes, totalBenchTime) << " nps";
}

void Uci::refreshTtPv(Ply depth) const {
    Position pos{position_};
    Score pos_score = pvScore;
    Ply d = depth;
    Ply pos_ply = 0;

    const UciMove* pv = pvMoves;
    for (UciMove move; (move = *pv++);) {
        auto o = tt.addr<TtSlot>(pos.zobrist());
        *o = TtSlot{pos.zobrist(), pos_score, pos_ply, ExactScore, d, move.from(), move.to(), false};
        ++tt.writes;

        //we cannot use makeZobrist() because of en passant legality validation
        pos.makeMoveNoEval(move.from(), move.to());
        pos_score = -pos_score;
        d = d-1;
        pos_ply = pos_ply+1;
    }
}

void Uci::info_perft_bestmove() {
    Output ob{this};
    info_nps(ob);
    ob << "bestmove 0000";
    limits.clear();
}

void Uci::readyok() const {
    Output ob{this};
    info_fen(ob) << '\n';
    info_nps(ob);
    ob << "readyok";
}

ostream& Uci::nps(ostream& o) const {
    // avoid printing identical nps info in a row
    if (lastInfoNodes == limits.getNodes()) { return o; }
    lastInfoNodes = limits.getNodes();

    o << " nodes " << lastInfoNodes;

    auto elapsedTime = limits.elapsedSinceStart();
    if (elapsedTime >= 1ms) {
        o << " time " << elapsedTime << " nps " << ::nps(lastInfoNodes, elapsedTime);
    }

    return o;
}

ostream& Uci::info_nps(ostream& o) const {
    if (limits.getNodes() == 0) { lastInfoNodes = 0; return o; }
    if (lastInfoNodes == limits.getNodes()) { return o; }

    o << "info"; nps(o) << '\n';
    return o;
}

ostream& Uci::info_fen(ostream& o) const {
    o << "info" << position_.evaluate() << " fen " << position_;
    return o;
}

void Uci::info_iteration(Ply d) const {
    Output ob{this};
    ob << "info depth " << d; nps(ob);
}

void Uci::info_pv(Ply d) const {
    UciOutput ob{this};
    ob << "info depth " << d; nps(ob);
    ob << pvScore << " pv"; ob << pvMoves;
}

void Uci::info_perft_depth(Ply d, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << d << " perft " << perft; nps(ob);
}

void Uci::info_perft_currmove(int moveCount, const UciMove& currentMove, node_count_t perft) const {
    UciOutput ob{this};
    ob << "info currmovenumber " << moveCount << " currmove ";
    ob << currentMove << " perft " << perft; nps(ob);
}
