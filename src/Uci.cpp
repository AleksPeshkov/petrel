#include <utility>
#include "perft.hpp"
#include "search.hpp"
#include "System.hpp"
#include "Uci.hpp"

// std::ostringstream output buffer, flushed on destruction
class Output : public io::ostringstream {
protected:
    const Uci& uci;
public:
    Output (const Uci* u) : io::ostringstream{}, uci{*u} {}
    io::ostringstream& flush() { uci.output(str()); str(""); return *this; }
    ~Output () { flush(); }
};

// std::ostringstream UciMove capable output buffer, flushed on destruction
class UciOutput : public Output {
    Color colorToMove;
public:
    UciOutput(const Uci* u) : Output{u}, colorToMove{uci.colorToMove()} {}
    ChessVariant chessVariant() const { return uci.chessVariant(); }
    Color color() const { return colorToMove; }
    Color flipColor() { return Color{colorToMove.flip()}; }
    void resetRootColor() { colorToMove = uci.colorToMove(); }
};

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

// typesafe operator<< chaining
UciOutput& operator << (UciOutput& out, io::czstring message) {
    static_cast<ostream&>(out) << message; return out;
}

// convert move to UCI format
UciOutput& operator << (UciOutput& out, UciMove move) {
    bool isWhite{out.color().is(White)};
    out.flipColor();
    out << ' ';

    if (move.none()) {
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

        if (out.chessVariant().is(Orthodox)) {
            if (from.on(FileA)) { out << uciTo << Square{File{FileC}, Rank{uciFrom}}; return out; }
            if (from.on(FileH)) { out << uciTo << Square{File{FileG}, Rank{uciFrom}}; return out; }
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
    out << pv.score();
    auto moves = pv.moves();
    if (moves->none()) { return out; } // empty PV (no legal moves at root)

    {
        out << " pv";
        for (UciMove move; (move = *moves++).any(); ) {
            out << move;
        }
        out.resetRootColor();
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
    pid{System::getPid()},
    logStartTime{::timeNow()}
{
    inputLine.clear();
    bestmove_.clear();
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
        std::lock_guard<std::mutex> lock{bestmoveMutex};
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
        auto timeStamp = ::elapsedSince(logStartTime);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeStamp).count();
        auto us = (std::chrono::duration_cast<std::chrono::microseconds>(timeStamp) % 1000).count();
        logFile << pid << " " << ms << '.' << std::setfill('0') << std::setw(3) << us;
        logFile << " " << message << std::endl;

        // recover if the logFile is in a bad state
        if (!logFile) {
            logFile.clear();
            logFile.close();
            logFile.open(logFileName, std::ios::app);
            logFile << "*logFile recovered from a bad state" << std::endl;
        }
    }
}

void Uci::error(std::string_view message) const {
    if (message.empty()) { return; }

    std::cerr << "petrel " << pid << " " << message << std::endl;
    log('!' + std::string(message));
}

void Uci::info(std::string_view message) const {
    if (message.empty()) { return; }

    log('*' + std::string(message));
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
        ::trimTrailingWhitespace(newFileName);

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

        if (consume("true"))  { position_.setChessVariant(ChessVariant{Chess960}); return; }
        if (consume("false")) { position_.setChessVariant(ChessVariant{Orthodox}); return; }

        io::fail_rewind(inputLine);
        return;
    }

    limits.setoption(inputLine);
}

void Uci::setdebug() {
    if (!hasMoreInput()) {
        Output ob{this};
        ob << "info string debug is " << (isDebugOn ? "on" : "off");
        return;
    }

    if (consume("on"))  { isDebugOn = true; info("debug on"); return; }
    if (consume("off")) { isDebugOn = false; info("debug off"); return; }

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
    position_.setStartpos();
    repetitions.clear();
    repetitions.push(colorToMove(), position_.z());
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
    if (started) {
        std::this_thread::yield();
        return;
    }

    if (bestmove_.empty()) {
        error("search not started, send bestmove 0000");
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
        if (isDebugOn) { info("sending delayed bestmove: " + bestmove); }
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
    UciOutput ob{this};
    info_pv(ob);
}

UciOutput& Uci::info_pv(UciOutput& ob) const {
    ob << "info depth " << pv.depth();
    limits.nps(ob);
    ob << pv;
    return ob;
}

void Uci::info_bestmove() const {
    UciOutput ob{this};
    auto delayed = limits.shouldDelayBestmove();

    if (limits.hasNewNodes()) {
        info_pv(ob);
        if (delayed) { ob.flush(); } else { ob << '\n'; }
    }

    ob << "bestmove" << pv.move(0_ply);
    if (limits.canPonder() && pv.move(1_ply).any()) {
        ob << " ponder" << pv.move(1_ply);
    }

    if (delayed) {
        {
            std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
            if (!bestmove_.empty()) { error("old bestmove ignored: " + bestmove_); }
            bestmove_ = std::move(ob).str();
            // will be sent later by 'stop' or 'ponderhit'
        }
        ob.str("");
        if (isDebugOn) { info("*bestmove output delayed: " + bestmove_); }
    }
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
    limits.info_nps(ob);
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
    limits.info_nps(ob);
    ob << "bestmove 0000";
}

void Uci::info_perft_depth(Ply depth, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << depth << " perft " << perft;
}

void Uci::info_perft_currmove(int moveCount, UciMove currentMove, node_count_t perft) const {
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

    node_count_t benchNodes = 0;
    auto benchStart = ::timeNow();

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
