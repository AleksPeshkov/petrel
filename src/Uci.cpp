#include "chrono.hpp"
#include "Uci.hpp"
#include "UciSearchLimits.hpp"
#include "Node.hpp"
#include "NodePerft.hpp"
#include "TimerManager.hpp"

template<class BasicLockable>
class OutputBuffer : public std::ostringstream {
    io::ostream& out;
    BasicLockable& lock;
    typedef std::lock_guard<decltype(lock)> Guard;
public:
    OutputBuffer (io::ostream& o, BasicLockable& l) : std::ostringstream{}, out(o), lock(l) {}
    ~OutputBuffer () { Guard g{lock}; out << str() << std::flush; }
};

#define OUTPUT(ob) OutputBuffer<decltype(outLock)> ob(out, outLock)

namespace {
    io::istream& operator >> (io::istream& in, TimeInterval& timeInterval) {
        unsigned long msecs;
        if (in >> msecs) {
            timeInterval = duration_cast<TimeInterval>(milliseconds{msecs} );
        }
        return in;
    }

    io::ostream& operator << (io::ostream& out, TimeInterval& timeInterval) {
        if (timeInterval < 1ms) { return out; }
        return out << " time " << duration_cast<milliseconds>(timeInterval).count();
    }

    template <typename nodes_type, typename duration_type>
    constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
        return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
    }

    template <typename T>
    static T mebi(T bytes) { return bytes / (1024 * 1024); }

    template <typename T>
    static constexpr T permil(T n, T m) { return (n * 1000) / m; }

    ostream& uci_error(ostream& err, io::istream& context) {
        return err << "parsing error: " << context.rdbuf() << std::endl;
    }
}

void Uci::processCommands(io::istream& in, ostream& err) {
    for (std::string currentLine; std::getline(in, currentLine); ) {
        inputLine.clear(); //clear error state from the previous line
        inputLine.str(std::move(currentLine));
        inputLine >> std::ws;

        if      (consume("go"))        { go(); }
        else if (consume("position"))  { position(); }
        else if (consume("stop"))      { mainSearchThread.stop(); }
        else if (consume("ponderhit")) { ponderhit(); }
        else if (consume("isready"))   { readyok(); }
        else if (consume("setoption")) { setoption(); }
        else if (consume("set"))       { setoption(); }
        else if (consume("ucinewgame")){ ucinewgame(); }
        else if (consume("uci"))       { uciok(); }
        else if (consume("perft"))     { goPerft(); }
        else if (consume("quit"))      { break; }
        else if (consume("exit"))      { break; }

        if (hasMoreInput()) {
            uci_error(err, inputLine);
        }
    }
}

void Uci::ucinewgame() {
    if (!isReady()) {
        io::fail_rewind(inputLine);
        return;
    }

    root.newGame();
    root.setStartpos();
}

void Uci::uciok() const {
    bool isChess960 = root.chessVariant().is(Chess960);

    OUTPUT(ob);
    ob << "id name petrel\n";
    ob << "id author Aleks Peshkov\n";
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "option name Ponder type check default " << (canPonder ? "true" : "false") << '\n';
    ob << "option name Hash type spin"
       << " min "     << ::mebi(root.tt.minSize())
       << " max "     << ::mebi(root.tt.maxSize())
       << " default " << ::mebi(root.tt.size())
       << '\n';
    ob << "uciok\n";
}

void Uci::setoption() {
    consume("name");

    if (consume("UCI_Chess960")) {
        consume("value");

        if (consume("true"))  { root.setChessVariant(Chess960); return; }
        if (consume("false")) { root.setChessVariant(Orthodox); return; }

        io::fail_rewind(inputLine);
        return;
    }

    if (consume("Ponder")) {
        consume("value");

        if (consume("true"))  { canPonder = true; return; }
        if (consume("false")) { canPonder = false; return; }

        io::fail_rewind(inputLine);
        return;
    }

    if (consume("Hash")) {
        consume("value");
        setHash();
        return;
    }
}

void Uci::setHash() {
    if (!isReady()) {
        io::fail_rewind(inputLine);
        return;
    }

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

    root.setHash(quantity);
}

void Uci::position() {
    if (!hasMoreInput()) {
        infoPosition();
        return;
    }

    if (consume("startpos")) { root.setStartpos(); }
    if (consume("fen")) { root.readFen(inputLine); }

    consume("moves");
    root.playMoves(inputLine);
}

void Uci::go() {
    if (!isReady()) {
        io::fail_rewind(inputLine);
        return;
    }

    {
        auto whiteSide = root.sideOf(White);
        auto blackSide = root.sideOf(Black);

        root.limits = {};
        while (inputLine >> std::ws, !inputLine.eof()) {
            if      (consume("depth"))    { inputLine >> root.limits.depth; }
            else if (consume("nodes"))    { inputLine >> root.limits.nodes; }
            else if (consume("movetime")) { inputLine >> root.limits.movetime; }
            else if (consume("wtime"))    { inputLine >> root.limits.time[whiteSide]; }
            else if (consume("btime"))    { inputLine >> root.limits.time[blackSide]; }
            else if (consume("winc"))     { inputLine >> root.limits.inc[whiteSide]; }
            else if (consume("binc"))     { inputLine >> root.limits.inc[blackSide]; }
            else if (consume("movestogo")){ inputLine >> root.limits.movestogo; }
            else if (consume("mate"))     { inputLine >> root.limits.mate; } // TODO: implement mate in n moves
            else if (consume("ponder"))   { root.limits.ponder = true; }
            else if (consume("infinite")) { root.limits.infinite = true; }
            else if (consume("searchmoves")) { root.limitMoves(inputLine); }
            else { io::fail(inputLine); return; }
        }
    }

    auto timeInterval = root.limits.calculateThinkingTime(canPonder);

    mainSearchThread.start([this] {
        Node{root}.searchRoot();
        bestmove();
    });

    if (timeInterval != 0ms) {
        setDeadline(timeInterval);
    }
}

void Uci::ponderhit() {
    root.searchStartTime = ::timeNow();
    root.limits.ponder = false;

    if (root.limits.movetime != 0ms) {
        setDeadline(root.limits.movetime);
        return;
    }

    auto timeInterval = root.limits.calculateThinkingTime(canPonder);
    if (timeInterval != 0ms) {
        timeInterval -= ::elapsedSince(root.searchStartTime);
        setDeadline(timeInterval);
    }
}

void Uci::setDeadline(TimeInterval timeInterval) {
    auto id = mainSearchThread.getTaskId();
    auto deadlineTask = [this, id]() { mainSearchThread.stop(id); };

    // stop immediately
    if (timeInterval < 1ms) { deadlineTask(); return; }

    The_timerManager.schedule(std::max<TimeInterval>(1ms, timeInterval - 1ms), deadlineTask);
}

void Uci::goPerft() {
    if (!isReady()) {
        io::fail_rewind(inputLine);
        return;
    }

    Ply depth;
    inputLine >> depth;
    depth = std::min<Ply>(depth, 18); // current Tt implementation limit

    mainSearchThread.start([this, depth] {
        NodePerft{root, depth}.visitRoot();
        perft_finish();
    } );
}

void Uci::readyok() const {
    OUTPUT(ob);
    if (isReady()) {
        isreadyWaiting = false;
        ob << "readyok\n";
    }
    else {
        isreadyWaiting = true;
    }
}

void Uci::info_nps_readyok() const {
    if (isreadyWaiting) {
        std::ostringstream ob;
        info_nps(ob);
        ob << "readyok\n";

        if (outLock.try_lock()) {
            if (isreadyWaiting) {
                isreadyWaiting = false;
                out << ob.str() << std::flush;
            }
            outLock.unlock();
        }
    }
}

ostream& Uci::nps(ostream& o) const {
    if (lastInfoNodes == root.nodeCounter) {
        return o;
    }
    lastInfoNodes = root.nodeCounter;

    auto timeInterval = ::elapsedSince(root.searchStartTime);

    o << " nodes " << lastInfoNodes << timeInterval << " nps " << ::nps(lastInfoNodes, timeInterval);
    return o;
}

ostream& Uci::info_nps(ostream& o) const {
    std::ostringstream buffer;
    nps(buffer);

    if (!buffer.str().empty()) {
        o << "info" << buffer.str() << '\n';
    }
    return o;
}

void Uci::bestmove() const {
    OUTPUT(ob);
    if (root.tt.reads > 0) {
        ob << "info";
        ob << " hwrites " << root.tt.writes;
        ob << " hhits " << root.tt.hits;
        ob << " hreads " << root.tt.reads;
        ob << " hhitratio " << ::permil(root.tt.hits, root.tt.reads);
        ob << '\n';
    }
    info_nps(ob);
    ob << "bestmove " << root.pvMoves[0];
    if (canPonder && root.pvMoves[1]) {
        ob << " ponder " << root.pvMoves[1];
    }
    ob << '\n';
    lastInfoNodes = 0;
}

void Uci::info_iteration(Ply draft) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob) << '\n';
}

void Uci::info_pv(Ply draft, Score score) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob) << score << " pv" << root.pvMoves << '\n';
}

void Uci::perft_depth(Ply draft, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info depth " << draft << " perft " << perft; nps(ob) << '\n';
}

void Uci::perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info currmovenumber " << moveCount << " currmove " << currentMove << " perft " << perft;
    nps(ob) << '\n';
}

void Uci::perft_finish() const {
    OUTPUT(ob);
    info_nps(ob);
    ob << "bestmove 0000\n";
}

void Uci::infoPosition() const {
    OUTPUT(ob);
    ob << "info fen " << root << '\n';
    ob << "info" << root.evaluate() << '\n';
}
