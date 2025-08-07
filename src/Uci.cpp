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

void Uci::operator() (io::istream& in, ostream& err) {
    for (std::string currentLine; std::getline(in, currentLine); ) {
        command.clear(); //clear state from the previous command
        command.str(std::move(currentLine));
        command >> std::ws;

        if      (next("go"))        { go(); }
        else if (next("position"))  { position(); }
        else if (next("stop"))      { searchThread.stop(); }
        else if (next("isready"))   { readyok(); }
        else if (next("setoption")) { setoption(); }
        else if (next("set"))       { setoption(); }
        else if (next("ucinewgame")){ ucinewgame(); }
        else if (next("uci"))       { uciok(); }
        else if (next("perft"))     { goPerft(); }
        else if (next("quit"))      { break; }
        else if (next("exit"))      { break; }

        //parsing error detected or something left unparsed
        if (!nextNothing()) {
            uci_error(err, command);
        }
    }
}

void Uci::ucinewgame() {
    if (!isReady()) {
        io::fail_rewind(command);
        return;
    }

    root.newGame();
    root.setStartpos();
}

void Uci::setoption() {
    next("name");

    if (next("UCI_Chess960")) {
        next("value");

        if (next("true"))  { root.setChessVariant(Chess960); return; }
        if (next("false")) { root.setChessVariant(Orthodox); return; }

        io::fail_rewind(command);
        return;
    }

    if (next("Hash")) {
        next("value");
        setHash();
        return;
    }

}

void Uci::setHash() {
    if (!isReady()) {
        io::fail_rewind(command);
        return;
    }

    size_t quantity = 0;
    command >> quantity;
    if (!command) {
        io::fail_rewind(command);
        return;
    }

    io::char_type unit = 'm';
    command >> unit;

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
            io::fail_rewind(command);
            return;
        }
    }

    root.setHash(quantity);
}

void Uci::position() {
    if (nextNothing()) {
        infoPosition();
        return;
    }

    if (next("startpos")) { root.setStartpos(); }
    if (next("fen")) { root.readFen(command); }

    next("moves");
    root.playMoves(command);
}

void Uci::go() {
    if (!isReady()) {
        io::fail_rewind(command);
        return;
    }

    {
        auto whiteSide = root.sideOf(White);
        auto blackSide = root.sideOf(Black);

        root.limits = {};
        while (command >> std::ws, !command.eof()) {
            if      (next("depth"))    { command >> root.limits.depth; }
            else if (next("nodes"))    { command >> root.limits.nodes; }
            else if (next("movetime")) { command >> root.limits.movetime; }
            else if (next("wtime"))    { command >> root.limits.time[whiteSide]; }
            else if (next("btime"))    { command >> root.limits.time[blackSide]; }
            else if (next("winc"))     { command >> root.limits.inc[whiteSide]; }
            else if (next("binc"))     { command >> root.limits.inc[blackSide]; }
            else if (next("movestogo")){ command >> root.limits.movestogo; }
            else if (next("mate"))     { command >> root.limits.mate; } // TODO: implement mate in n moves
            else if (next("ponder"))   { root.limits.ponder = true; } // TODO: implement ponder
            else if (next("infinite")) { root.limits.infinite = true; }
            else if (next("searchmoves")) { root.limitMoves(command); }
            else { io::fail(command); return; }
        }
        root.limits.calculateThinkingTime();
    }

    auto id = searchThread.start([this] {
        Node{root}.searchRoot();
        bestmove();
    });

    auto timeInterval = root.limits.thinkingTime;
    if (timeInterval > 0ms) {
        auto deadlineTask = [this, id]() { searchThread.stop(id); };
        The_timerManager.schedule(std::max<TimeInterval>(1ms, timeInterval - 1ms), deadlineTask);
    }
}

void Uci::goPerft() {
    if (!isReady()) {
        io::fail_rewind(command);
        return;
    }

    Ply depth;
    command >> depth;
    depth = std::min<Ply>(depth, 18); // current Tt implementation limit

    searchThread.start([this, depth] {
        NodePerft{root, depth}.visitRoot();
        perft_finish();
    } );
}

void Uci::uciok() const {
    bool isChess960 = root.chessVariant().is(Chess960);

    OUTPUT(ob);
    ob << "id name petrel\n";
    ob << "id author Aleks Peshkov\n";
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "option name Hash type spin"
       << " min "     << ::mebi(root.tt.minSize())
       << " max "     << ::mebi(root.tt.maxSize())
       << " default " << ::mebi(root.tt.size())
       << '\n';
    ob << "uciok\n";
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

void Uci::infoNpsReadyok() const {
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

    if (root.tt.reads > 0) {
        o << " hwrites " << root.tt.writes;
        o << " hhits " << root.tt.hits;
        o << " hreads " << root.tt.reads;
        o << " hhitratio " << ::permil(root.tt.hits, root.tt.reads);
    }
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
    info_nps(ob);
    ob << "bestmove " << root.pvMoves[0] << '\n';
    lastInfoNodes = 0;
}

void Uci::infoIterationEnd(Ply draft) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob) << '\n';
}

void Uci::infoNewPv(Ply draft, Score score) const {
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
