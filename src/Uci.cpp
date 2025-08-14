#include "chrono.hpp"
#include "Uci.hpp"
#include "UciSearchLimits.hpp"
#include "Node.hpp"
#include "NodePerft.hpp"

#ifdef DEBUG
extern const Uci* The_uci;
void log(const std::string& message) {
    if (The_uci) { The_uci->log(message); }
}
#endif

template<class BasicLockable>
class OutputBuffer : public std::ostringstream {
    io::ostream& out;
    BasicLockable& outLock;
    const Uci& uci;

public:
    OutputBuffer (io::ostream& o, BasicLockable& l, const Uci& u)
        : std::ostringstream{}, out{o}, outLock{l}, uci{u}
    {}

    ~OutputBuffer () {
        std::string message = str();
        if (message.empty()) { return; }

        {
            std::lock_guard<decltype(outLock)> lock{outLock};
            out << message << std::flush;
        }
        uci.log(message);
    }
};

#define OUTPUT(ob) OutputBuffer<decltype(outLock)> ob(out, outLock, *this)

namespace {
    io::istream& operator >> (io::istream& in, TimeInterval& timeInterval) {
        unsigned long msecs;
        if (in >> msecs) {
            timeInterval = duration_cast<TimeInterval>(milliseconds{msecs} );
        }
        return in;
    }

    io::ostream& operator << (io::ostream& out, const TimeInterval& timeInterval) {
        return out << duration_cast<milliseconds>(timeInterval).count();
    }

    template <typename nodes_type, typename duration_type>
    constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
        return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
    }

    template <typename T>
    static T mebi(T bytes) { return bytes / (1024 * 1024); }

    template <typename T>
    static constexpr T permil(T n, T m) { return (n * 1000) / m; }
}

void Uci::processCommands(io::istream& in) {
    for (std::string currentLine; std::getline(in, currentLine); ) {
        log('>' + currentLine + '\n');

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
            std::string unparsedInput;
            std::getline(inputLine, unparsedInput);
            log("#parsing error: " + unparsedInput + '\n');
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
    ob << "option name Debug Log File type string default " << (logFileName.empty() ? "<empty>" : logFileName) << '\n';
    ob << "option name Hash type spin"
       << " min "     << ::mebi(root.tt.minSize())
       << " max "     << ::mebi(root.tt.maxSize())
       << " default " << ::mebi(root.tt.size())
       << '\n';
    ob << "option name Ponder type check default " << (root.limits.canPonder ? "true" : "false") << '\n';
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "option name Move Overhead type spin min 0 default " << root.limits.moveOverhead << '\n';
    ob << "uciok\n";
}

void Uci::setoption() {
    consume("name");

    if (consume("Debug Log File")) {
        consume("value");

        inputLine >> std::ws;
        logFileName.clear();
        std::getline(inputLine, logFileName);
        if (logFileName == "<empty>") { logFileName.clear(); }

        if (logFileName.empty()) {
            logFile.close();
        } else {
            logFile.open(logFileName, std::ios::app);
        }
        return;
    }

    if (consume("Hash")) {
        consume("value");
        setHash();
        return;
    }

    if (consume("Ponder")) {
        consume("value");

        if (consume("true"))  { root.limits.canPonder = true; return; }
        if (consume("false")) { root.limits.canPonder = false; return; }

        io::fail_rewind(inputLine);
        return;
    }

    if (consume("UCI_Chess960")) {
        consume("value");

        if (consume("true"))  { root.setChessVariant(Chess960); return; }
        if (consume("false")) { root.setChessVariant(Orthodox); return; }

        io::fail_rewind(inputLine);
        return;
    }

    if (consume("Move Overhead")) {
        consume("value");

        inputLine >> root.limits.moveOverhead;
        if (!inputLine) { io::fail_rewind(inputLine); }
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

        root.limits.clear();
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

    mainSearchThread.start([this] {
        Node{root}.searchRoot();
        bestmove();
    });

    root.limits.setDeadline(mainSearchThread);
}

void Uci::ponderhit() {
    root.limits.ponderhit(mainSearchThread);
}

void Uci::goPerft() {
    if (!isReady()) {
        io::fail_rewind(inputLine);
        return;
    }

    Ply depth;
    inputLine >> depth;
    depth = std::min<Ply>(depth, 18); // current Tt implementation limit

    root.limits.clear();
    mainSearchThread.start([this, depth] {
        NodePerft{root, depth}.visitRoot();
        info_perft_bestmove();
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
        std::unique_lock<std::mutex> lock(outLock, std::try_to_lock);
        if (lock.owns_lock() && isreadyWaiting) {
            isreadyWaiting = false;

            std::ostringstream ob;
            info_nps(ob);
            ob << "readyok\n";

            out << ob.str() << std::flush;
        }
    }
}

ostream& Uci::nps(ostream& o) const {
    if (lastInfoNodes == root.nodeCounter) {
        return o;
    }

    lastInfoNodes = root.nodeCounter;
    o << " nodes " << lastInfoNodes;

    auto elapsedTime = ::elapsedSince(root.limits.searchStartTime);
    if (elapsedTime >= 1ms) {
        o << " time " << elapsedTime << " nps " << ::nps(lastInfoNodes, elapsedTime);
    }

    return o;
}

ostream& Uci::info_nps(ostream& o) const {
    if (root.tt.reads > 0) {
        o << "info";
        o << " hwrites " << root.tt.writes;
        o << " hhits " << root.tt.hits;
        o << " hreads " << root.tt.reads;
        o << " hhitratio " << ::permil(root.tt.hits, root.tt.reads);
        o << '\n';
    }

    std::ostringstream ob;
    nps(ob);
    if (!ob.str().empty()) {
        o << "info" << ob.str() << '\n';
    }

    return o;
}

ostream& Uci::info_fen(ostream& o) const {
    if (!logFile.is_open()) { return o; }
    o << "info fen " << root << '\n';
    return o;
}

void Uci::bestmove() const {
    OUTPUT(ob);
    info_fen(ob);
    info_nps(ob);
    ob << "bestmove " << root.pvMoves[0];
    if (root.limits.canPonder && root.pvMoves[1]) {
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

void Uci::info_perft_depth(Ply draft, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info depth " << draft << " perft " << perft; nps(ob) << '\n';
}

void Uci::info_perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info currmovenumber " << moveCount << " currmove " << currentMove << " perft " << perft;
    nps(ob) << '\n';
}

void Uci::info_perft_bestmove() const {
    OUTPUT(ob);
    info_nps(ob);
    ob << "bestmove 0000\n";
}

void Uci::log(const std::string& message) const {
    if (!logFile.is_open()) { return; }
    if (message.empty()) { return; } // Skip empty messages

    {
        Guard lock{outLock}; // Lock the log mutex
        logFile << message << std::flush; // Write and flush the message

        // Recover if the log is in a bad state
        if (!logFile) {
            logFile.clear(); // Clear error flags
            logFile.close(); // Close the file
            logFile.open(logFileName, std::ios::app); // Reopen
        }
    }
}
