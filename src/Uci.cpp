#include <utility>
#include "perft.hpp"
#include "search.hpp"
#include "System.hpp"
#include "Uci.hpp"

namespace { // anonymous namespace

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

// typesafe operator<< chaining
UciOutput& operator << (UciOutput& out, io::czstring message) {
    static_cast<ostream&>(out) << message; return out;
}

// convert move to UCI format
UciOutput& operator << (UciOutput& out, UciMove move) {
    if (!move) {
        io::info("illegal move 0000 printed");
        out << "0000";
        return out;
    }

    Square from{move.from()};
    Square to{move.to()};

    bool isWhite{out.color() == White};
    Square uciFrom{isWhite ? from : ~from};
    Square uciTo{isWhite ? to : ~to};

    if (!move.isSpecial()) {
        out << uciFrom << uciTo;
        return out;
    }

    //pawn promotion
    if (from.on(Rank7)) {
        //the type of a promoted pawn piece encoded in place of move to's rank
        uciTo = Square{File{to}, isWhite ? Rank8 : Rank1};
        out << uciFrom << uciTo << PromoType{::promoTypeFrom(Rank{to})};
        return out;
    }

    //en passant capture
    if (from.on(Rank5)) {
        //en passant capture move internally encoded as pawn captures pawn
        assert (to.on(Rank5));
        out << uciFrom << Square{File{to}, isWhite ? Rank6 : Rank3};
        return out;
    }

    //castling
    if (from.on(Rank1)) {
        //castling move internally encoded as the rook captures the king

        if (out.chessVariant() == Orthodox) {
            if (from.on(FileA)) { out << uciTo << Square{FileC, Rank{uciFrom}}; return out; }
            if (from.on(FileH)) { out << uciTo << Square{FileG, Rank{uciFrom}}; return out; }
        }

        // Chess960:
        out << uciTo << uciFrom;
        return out;
    }

    //should never happen
    assert (false);
    io::error("invalid move in UCI output");
    out << "0000";
    return out;
}

UciOutput& operator << (UciOutput& out, const PvMoves& pvMoves) {
    auto moves = static_cast<const UciMove*>(pvMoves);
    out << " pv";
    for (UciMove move; (move = *moves++); ) {
        out << " "; out << move; out.flipColor();
    }
    return out;
}

static constexpr size_t mebibyte = 1024 * 1024;

template <typename T>
static T mebi(T bytes) { return bytes / mebibyte; }

template <typename T>
static constexpr T permil(T n, T m) { return (n * 1000) / m; }

} // anonymous namespace

Uci::Uci(ostream &o) :
    tt(16 * mebibyte),
    inputLine{std::string(2048, '\0')}, // preallocate 2048 bytes (~200 full moves)
    out{o},
    bestmove_(sizeof("bestmove a7a8q ponder h2h1q"), '\0'),
    pid{System::getPid()}
{
    inputLine.clear();
    bestmove_.clear();
    ucinewgame();
}

void Uci::output(std::string_view message) const {
    if (message.empty()) { return; }

    {
        std::lock_guard<decltype(outMutex)> lock{outMutex};
        out << message << std::endl;
    }

    if (isDebugOn) { log('<' + std::string(message)); }
}

void Uci::log(std::string_view message) const {
    if (!logFile.is_open()) { return; }

    {
        std::lock_guard<decltype(logMutex)> lock{logMutex};
        logFile << pid << " " << message << std::endl;

        // recover if the logFile is in a bad state
        if (!logFile) {
            logFile.clear();
            logFile.close();
            logFile.open(logFileName, std::ios::app);
            logFile << "#logFile recovered from a bad state" << std::endl;
        }
    }
}

void Uci::error(std::string_view message) const {
    if (message.empty()) { return; }

    std::cerr << "petrel " << pid << " " << message << std::endl;
    log('!' + std::string(message));

    Output ob{this};
    ob << "info string " << message;
}

void Uci::info(std::string_view message) const {
    if (message.empty()) { return; }

    log('*' + std::string(message));

    Output ob{this};
    ob << "info string " << message;
}

void Uci::processInput(istream& in) {
    std::string currentLine(2048, '\0'); // preallocate 2048 bytes (~200 full moves)
    while (std::getline(in, currentLine)) {
        if (isDebugOn) { log('>' + currentLine); }

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
        else if (consume("debug"))     { setdebug(); }
        else if (consume("perft"))     { goPerft(); }
        else if (consume("bench"))     { bench(); }
        else if (consume("quit"))      { stop(); break; }
        else if (consume("exit"))      { stop(); break; }

        if (hasMoreInput()) {
            std::string unparsedInput;
            inputLine.clear();
            std::getline(inputLine, unparsedInput);
            if (isDebugOn) { log('>' + currentLine); }
            error("unparsed input->" + unparsedInput);
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
                error("failed opening Deubg Log File: " + newFileName);
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

    tt.setSize(quantity);
    newGame();
}

void Uci::setdebug() {
    if (!hasMoreInput()) {
        Output ob{this};
        ob << "info string debug is " << (isDebugOn ? "on" : "off");
        return;
    }

    if (consume("on"))  { isDebugOn = true; log("#debug on"); return; }
    if (consume("off")) { isDebugOn = false; log("#debug off"); return; }

    io::fail_rewind(inputLine);
    return;
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

    mainSearchThread.waitReady();

#ifdef ENABLE_ASSERT_LOGGING
    debugPosition = inputLine.str();
#endif

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
#ifdef ENABLE_ASSERT_LOGGING
    debugGo = inputLine.str();
#endif

    newSearch();
    limits.go(inputLine, position_.sideOf(White));
    if (consume("searchmoves")) { position_.limitMoves(inputLine); }

    auto started = mainSearchThread.start([this] {
        Node{position_, *this}.searchRoot();
        info_bestmove();
    });
    if (!started) {
        if (bestmove_.empty()) {
            error("search not started, send bestmove 0000");
            info_bestmove();
        } else {
            error("search not started, bestmove not empty:" + bestmove_);
            sendDelayedBestMove();
        }
    }
    std::this_thread::yield();
}

void Uci::sendDelayedBestMove() const {
    std::string bestmove;
    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        bestmove = std::exchange(bestmove_, "");
    }

    if (!bestmove.empty()) {
        log("#Sending delayed bestmove: " + bestmove);
    }
    output(bestmove); // usually empty
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

// update TT with PV (in case it have been overwritten)
void Uci::refreshTtPv(Ply depth) const {
    Position pos{position_};
    Score score = pvScore;
    Ply ply = 0_ply;

    const UciMove* moves = pvMoves;
    for (UciMove move; (move = *moves++);) {
        auto o = tt.addr<TtSlot>(pos.zobrist());
        *o = TtSlot{pos.zobrist(), score, ply, ExactScore, depth, move.from(), move.to(), false};
        ++tt.writes;

        //we cannot use makeZobrist() because of en passant legality validation
        pos.makeMove(move.from(), move.to());
        score = -score;
        depth = Ply{depth-1};
        ply = Ply{ply+1};
    }
}

void Uci::info_bestmove() const {
    // avoid printing identical info lines
    if (lastInfoNodes != limits.getNodes()) {
        UciOutput o{this};
        o << "info"; nps(o); o << pvScore; o << pvMoves;
        // flushed by destructor
    }

    UciOutput ob{this};
    ob << "bestmove "; ob << pvMoves[0];
    if (limits.canPonder && pvMoves[1]) {
        ob << " ponder "; ob.flipColor(); ob << pvMoves[1];
    }

    if (!limits.shouldDelayBestmove()) { return; } // bestmove flushed by destructor

    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (!bestmove_.empty()) { log("#Old bestmove ignored: " + bestmove_); }
        bestmove_ =  std::move(ob).str();
        // will be sent later by 'stop' or 'ponderhit'
    }
    ob.str("");
    log("#Bestmove output delayed: " + bestmove_);
}

void Uci::info_readyok() const {
    Output ob{this};
#ifndef NDEBUG
    info_fen(ob) << '\n';
#endif
    info_nps(ob);
    ob << "readyok";
}

ostream& Uci::info_fen(ostream& o) const {
    o << "info" << position_.evaluate() << " fen " << position_;
    return o;
}

void Uci::info_pv(Ply depth) const {
    UciOutput ob{this};
    ob << "info depth " << depth; nps(ob); ob << pvScore; ob << pvMoves;
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
    info_nps(ob);
    ob << "bestmove 0000";
}

void Uci::info_perft_depth(Ply depth, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << depth << " perft " << perft; nps(ob);
}

void Uci::info_perft_currmove(int moveCount, UciMove currentMove, node_count_t perft) const {
    UciOutput ob{this};
    ob << "info currmovenumber " << moveCount << " currmove "; ob << currentMove << " perft " << perft; nps(ob);
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
        goLimits = "depth 18 nodes 50000000"; // default
#else
        goLimits = "depth 8 nodes 100000"; // default for slow build
#endif
    }

    std::istringstream is{goLimits};

    static std::string_view positions[][2] = {
        {"3R1R2/K3k3/1p1nPb2/pN2P2N/nP1ppp2/4P3/6P1/4Qq1r w - - 0 1", "bm e1e2; id mate#5 talkchess.com/viewtopic.php?p=904264"}, // depth 13
        {"2b3r1/6pp/1kn2p2/7N/ppp1PN2/5P2/1PP2KPP/R7 b - - 1 28", "bm b6a5 talkchess.com/forum/viewtopic.php?t=85672"},
        {"2kr3r/Qbp1q1bp/1np3p1/5p2/2P1pP2/1PN3P1/PBK3BP/3RR3 w - - 0 21", "bm e1e4; id petrel 20251206"}, // depth 18
        {"6k1/p1rqbppp/1p2p3/nb1pP3/3P1NBP/PP4P1/5PN1/R2Q2K1 w - - 0 26", "bm f4e6; id petrel 20251231"}, // depth 16
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 25", "id kiwipete"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "id startpos"},
    };

    uciok();

    node_count_t benchNodes = 0;
    auto benchStart = ::timeNow();

    for (auto pos : positions) {
        std::string fen{pos[0]};
        inputLine.clear();
        inputLine.str(fen);

        position_.readFen(inputLine);
        if (hasMoreInput()) {
            log("#Error parsing bench fen: " + fen);
            continue;
        }

        {
            Output ob{this};
            ob << "\n"; info_fen(ob); ob << " ; " << pos[1];
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

    Output ob{this};
    ob << "\n" << benchNodes << " nodes " << ::nps(benchNodes, elapsedSince(benchStart)) << " nps";
}
