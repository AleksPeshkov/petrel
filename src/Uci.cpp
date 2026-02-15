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

istream& operator >> (istream& in, TimeInterval& timeInterval) {
    int msecs;
    if (in >> msecs) {
        timeInterval = std::chrono::duration_cast<TimeInterval>(std::chrono::milliseconds{msecs} );
    }
    return in;
}

ostream& operator << (ostream& out, const TimeInterval& timeInterval) {
    return out << std::chrono::duration_cast<std::chrono::milliseconds>(timeInterval).count();
}

template <typename nodes_type, typename duration_type>
constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
    return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
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
    bool isWhite{out.color() == White};
    out.flipColor();
    out << ' ';

    if (!move) {
        io::info("illegal move 0000 printed");
        out << "0000";
        return out;
    }

    Square from{move.from()};
    Square to{move.to()};

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

UciOutput& operator << (UciOutput& out, const PrincipalVariation& pv) {
    out << " depth " << pv.depth();
    out << pv.score();
    out << " pv";
    {
        auto moves = pv.moves();
        for (UciMove move; (move = *moves++); ) {
            out << move;
        }
    }
    return out;
}

static constexpr size_t mebibyte = 1024 * 1024;

template <typename T>
static T mebi(T bytes) { return bytes / mebibyte; }

template <typename T>
static constexpr T permil(T n, T m) { return (n * 1000) / m; }

} // anonymous namespace

void UciSearchLimits::newSearch() {
    searchStartTime_ = timeNow();

    nodes_ = 0;
    nodesLimit_ = NodeCountMax;
    nodesQuota_ = 0;
    lastInfoNodes_ = 0;

    infinite_ = false;
    timeout_.store(false, std::memory_order_seq_cst);
    pondering_.store(false, std::memory_order_release);

    time_ = {{ 0ms, 0ms }};
    inc_ = {{ 0ms, 0ms }};
    movetime_ = 0ms;
    movestogo_ = 0;

    timePool_ = UnlimitedTime;
    timeControl_ = ExactTime;
    easyMove_ = UciMove{};
    iterLowMaterialBonus_ = 0;

    maxDepth_ = MaxPly;
}

int UciSearchLimits::lookAheadMoves() const { return movestogo_ > 0 ? std::min(movestogo_, 16) : 16; }
TimeInterval UciSearchLimits::lookAheadTime(Side si) const { return time_[si] + inc_[si] * (lookAheadMoves() - 1); }
TimeInterval UciSearchLimits::averageMoveTime(Side si) const { return lookAheadTime(si) / lookAheadMoves(); }

void UciSearchLimits::setSearchDeadlines(const Position* p) {
    if (movetime_ > 0ms) {
        timePool_ = movetime_;
        timeControl_ = ExactTime;
        return;
    }

    auto noTimeLimits = time_[Side{My}] <= 0ms && inc_[Side{My}] <= 0ms;
    if (infinite_ || noTimeLimits) {
        timePool_ = UnlimitedTime;
        timeControl_ = ExactTime;
        return;
    }

    // [0..6] startpos = 6, queens exchanged = 4, R vs R endgame = 1
    int gamePhase = p ? p->gamePhase() : 4;
    iterLowMaterialBonus_ = 4 - std::clamp(gamePhase, 1, 5);

    // HardMove or HardDeadline may spend more than average move time
    auto optimumTime = averageMoveTime(Side{My}) + (canPonder_ ? averageMoveTime(Side{Op}) / 2 : 0ms);

    // allocate more time for the first out of book move in the game (fill up empty TT)
    if (isNewGame_) { optimumTime *= 13; optimumTime /= 8; isNewGame_ = false; }

    optimumTime *= static_cast<int>(HardMove) * HardDeadline;
    optimumTime /= static_cast<int>(NormalMove) * AverageTimeScale;
    optimumTime -= moveOverhead_;

    // can spend totalRatio/8 of all remaining time (including future time increments)
    auto totalRatio = 6 - std::clamp(gamePhase, 2, 4);

    auto maximumTime = lookAheadTime(Side{My}) * totalRatio / 8;
    maximumTime = std::min(time_[Side{My}] * 63/64, maximumTime);
    maximumTime = std::max(TimeInterval{0}, maximumTime - moveOverhead_);

    timePool_ = std::clamp(optimumTime, TimeInterval{0}, maximumTime);
    timeControl_ = EasyMove;
}

istream& UciSearchLimits::go(istream& in, Side white, const Position* p) {
    const Side black{~white};
    while (in >> std::ws, !in.eof()) {
        if      (io::consume(in, "depth"))    { in >> maxDepth_;    if (maxDepth_    < 0)   { maxDepth_    = 0_ply; } }
        else if (io::consume(in, "nodes"))    { in >> nodesLimit_;  if (nodesLimit_  < 0)   { nodesLimit_  = 0; } }
        else if (io::consume(in, "movetime")) { in >> movetime_;    if (movetime_    < 0ms) { movetime_    = 0ms; } }
        else if (io::consume(in, "wtime"))    { in >> time_[white]; if (time_[white] < 0ms) { time_[white] = 0ms; } }
        else if (io::consume(in, "btime"))    { in >> time_[black]; if (time_[black] < 0ms) { time_[black] = 0ms; } }
        else if (io::consume(in, "winc"))     { in >> inc_[white];  if (inc_[white]  < 0ms) { inc_[white]  = 0ms; }; }
        else if (io::consume(in, "binc"))     { in >> inc_[black];  if (inc_[black]  < 0ms) { inc_[black]  = 0ms; } }
        else if (io::consume(in, "movestogo")){ in >> movestogo_;   if (movestogo_   < 0)   { movestogo_   = 0; } }
        else if (io::consume(in, "mate"))     { in >> maxDepth_; maxDepth_ = Ply{std::abs(maxDepth_) * 2 + 1}; } // TODO: implement mate in n moves
        else if (io::consume(in, "ponder"))   { pondering_.store(true, std::memory_order_release); }
        else if (io::consume(in, "infinite")) { infinite_ =  true; }
        else { break; }
    }

    setSearchDeadlines(p);
    return in;
}

void UciSearchLimits::stop() {
    infinite_ = false;
    timeout_.store(true, std::memory_order_seq_cst);
    pondering_.store(false, std::memory_order_release);
}

void UciSearchLimits::ponderhit() {
    pondering_.store(false, std::memory_order_release);
}

ostream& UciSearchLimits::info_nps(ostream& out) const {
    lastInfoNodes_ = getNodes();
    out << "info nodes " << lastInfoNodes_;

    auto elapsedTime = elapsedSinceStart();
    if (elapsedTime >= 1ms) {
        out << " time " << elapsedTime << " nps " << ::nps(lastInfoNodes_, elapsedTime);
    }
    return out;
}

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

void Uci::newGame() {
    limits.newGame();
    tt.newGame();
    counterMove.clear();
    followMove.clear();
}

void Uci::newSearch() {
    {
        std::lock_guard<std::mutex> lock{bestmoveMutex};
        if (!bestmove_.empty()) {
            log("#New search started, but bestmove is not empty: " + bestmove_);
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
    ob << "option name Move Overhead type spin min 0 max 10000 default " << limits.moveOverhead_ << '\n';
    ob << "option name Ponder type check default " << (limits.canPonder_ ? "true" : "false") << '\n';
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

        inputLine >> limits.moveOverhead_;
        if (limits.moveOverhead_ < 0ms) { limits.moveOverhead_ = UciSearchLimits::MoveOverheadDefault; }

        if (!inputLine) { io::fail_rewind(inputLine); }
        return;
    }

    if (consume("Ponder")) {
        consume("value");

        if (consume("true"))  { limits.canPonder_ = true; return; }
        if (consume("false")) { limits.canPonder_ = false; return; }

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
    tt.prefetch<TtSlot>(position_.zobrist());
}

void Uci::go() {
    newSearch();

#ifdef ENABLE_ASSERT_LOGGING
    debugGo = inputLine.str();
#endif

    limits.go(inputLine, position_.sideOf(White), &position_);
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

void Uci::info_pv() const {
    // avoid printing identical info lines
    if (limits.hasNewNodes()) {
        UciOutput ob{this};
        limits.info_nps(ob); ob << pv;
    }
}

void Uci::info_bestmove() const {
    info_pv();

    UciOutput ob{this};
    ob << "bestmove" << pv.move(0_ply);
    if (limits.canPonder_ && pv.move(1_ply)) {
        ob << " ponder" << pv.move(1_ply);
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

ostream& Uci::info_fen(ostream& o) const {
    o << "info" << position_.evaluate() << " fen " << position_;
    return o;
}

void Uci::info_readyok() const {
    Output ob{this};
#ifndef NDEBUG
    info_fen(ob) << '\n';
#endif
    if (limits.hasNewNodes()) {
        limits.info_nps(ob) << '\n';
    }
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
        limits.info_nps(ob) << '\n';
    }
    ob << "bestmove 0000";
}

void Uci::info_perft_depth(Ply depth, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << depth << " perft " << perft;
}

void Uci::info_perft_currmove(int moveCount, UciMove currentMove, node_count_t perft) const {
    UciOutput ob{this};
    limits.info_nps(ob); ob << " currmovenumber " << moveCount << " currmove"; ob << currentMove << " perft " << perft;
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
