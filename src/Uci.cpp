#include "chrono.hpp"
#include "Uci.hpp"
#include "UciSearchLimits.hpp"
#include "Node.hpp"
#include "NodePerft.hpp"

class Output : public io::ostringstream {
    const Uci& uci;
public:
    Output (const Uci* u) : io::ostringstream{}, uci{*u} {}
    ~Output () { uci.output(str()); }
};

namespace {
    istream& operator >> (istream& in, TimeInterval& timeInterval) {
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

Uci::Uci(ostream &o) :
    root{*this},
    inputLine{std::string(1024, '\0')}, // preallocate 1024 bytes
    out{o}
{
    ucinewgame();
}

void Uci::processInput(istream& in) {
    std::string currentLine(1024, '\0'); // preallocate 1024 bytes
    while (std::getline(in, currentLine)) {

#ifndef NDEBUG
        log('>' + currentLine + '\n');
#endif

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
        else if (consume("perft"))     { goPerft(); }
        else if (consume("quit"))      { break; }
        else if (consume("exit"))      { break; }

        if (hasMoreInput()) {
            std::string unparsedInput;
            inputLine.clear();
            std::getline(inputLine, unparsedInput);

            log(currentLine + "\n#unparsed input->" + unparsedInput + '\n');
        }
    }
}

void Uci::ucinewgame() {
    root.newGame();
    root.setStartpos();
}

void Uci::uciok() const {
    bool isChess960 = root.chessVariant().is(Chess960);

    Output ob{this};
    ob << "id name " << io::app_version << '\n';
    ob << "id author Aleks Peshkov\n";
    ob << "option name Debug Log File type string default " << (logFileName.empty() ? "<empty>" : logFileName) << '\n';
    ob << "option name Hash type spin"
       << " min "     << ::mebi(root.tt.minSize())
       << " max "     << ::mebi(root.tt.maxSize())
       << " default " << ::mebi(root.tt.size())
       << '\n';
    ob << "option name Move Overhead type spin min 0 max 10000 default " << root.limits.moveOverhead << '\n';
    ob << "option name Ponder type check default " << (root.limits.canPonder ? "true" : "false") << '\n';
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "uciok\n";
}

void Uci::setoption() {
    consume("name");

    if (consume("Debug Log File")) {
        consume("value");

        inputLine >> std::ws;
        std::string newLogFileName;
        std::getline(inputLine, newLogFileName);

        if (newLogFileName == "<empty>") {
            logFileName.clear();
            logFile.close();
            return;
        }

        if (newLogFileName != logFileName) {
            logFile.close();
            logFileName = std::move(newLogFileName);
            logFile.open(logFileName, std::ios::app);
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

        inputLine >> root.limits.moveOverhead;
        if (root.limits.moveOverhead == 0ms) { root.limits.moveOverhead = 100us; }

        if (!inputLine) { io::fail_rewind(inputLine); }
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

    root.setHash(quantity);
}

void Uci::position() {
    if (!hasMoreInput()) {
        Output ob{this};
        info_fen(ob);
        return;
    }

    if (consume("startpos")) { root.setStartpos(); }
    if (consume("fen")) { root.readFen(inputLine); }

    consume("moves");
    root.playMoves(inputLine);
}

void Uci::go() {
    root.limits.clear();
    {
        auto whiteSide = root.sideOf(White);
        auto blackSide = root.sideOf(Black);

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
    root.limits.setSearchDeadline();

    mainSearchThread.start([this] {
        Node{root}.searchRoot();
        bestmove();
    });
}

void Uci::ponderhit() {
    root.limits.ponderhit();
}

void Uci::goPerft() {

    Ply depth;
    inputLine >> depth;
    depth = std::min<Ply>(depth, {18}); // current Tt implementation limit

    root.limits.clear();
    mainSearchThread.start([this, depth] {
        NodePerft{root, depth}.visitRoot();
        info_perft_bestmove();
    } );
}

void Uci::readyok() const {
    if (isStopped()) {
        mainSearchThread.waitReady();
    }

    {
        Output ob{this};
        info_nps(ob);
        ob << "readyok\n";
    }
}

ostream& Uci::nps(ostream& o) const {
    // avoid printing identical nps info in a row
    if (lastInfoNodes == root.nodeCounter) { return o; }
    lastInfoNodes = root.nodeCounter;

    o << " nodes " << lastInfoNodes;

    auto elapsedTime = ::elapsedSince(root.limits.searchStartTime);
    if (elapsedTime >= 1ms) {
        o << " time " << elapsedTime << " nps " << ::nps(lastInfoNodes, elapsedTime);
    }

    return o;
}

ostream& Uci::info_nps(ostream& o) const {
    if (lastInfoNodes == root.nodeCounter) { return o; }

    if (root.tt.reads > 0) {
        o << "info";
        o << " hwrites " << root.tt.writes;
        o << " hhits " << root.tt.hits;
        o << " hreads " << root.tt.reads;
        o << " hhitratio " << ::permil(root.tt.hits, root.tt.reads);
        o << '\n';
    }

    o << "info"; nps(o) << '\n';
    return o;
}

ostream& Uci::info_fen(ostream& o) const {
    o << "info" << root.evaluate() << " fen " << root << '\n';
    return o;
}

void Uci::bestmove() const {
    Output ob{this};
    info_fen(ob);
    info_nps(ob);
    ob << "bestmove " << root.pvMoves[0];
    if (root.limits.canPonder && root.pvMoves[1]) {
        ob << " ponder " << root.pvMoves[1];
    }
    ob << '\n';
}

void Uci::info_iteration(Ply draft) const {
    Output ob{this};
    ob << "info depth " << draft; nps(ob) << '\n';
}

void Uci::info_pv(Ply draft, Score score) const {
    Output ob{this};
    ob << "info depth " << draft; nps(ob) << score << " pv" << root.pvMoves << '\n';
}

void Uci::info_perft_depth(Ply draft, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << draft << " perft " << perft; nps(ob) << '\n';
}

void Uci::info_perft_currmove(int moveCount, const UciMove& currentMove, node_count_t perft) const {
    Output ob{this};
    ob << "info currmovenumber " << moveCount << " currmove " << currentMove << " perft " << perft;
    nps(ob) << '\n';
}

void Uci::info_perft_bestmove() const {
    Output ob{this};
    info_nps(ob);
    ob << "bestmove 0000\n";
}

void Uci::output(const std::string& message) const {
    if (message.empty()) { return; }

    {
        ScopedLock lock{mutex};
        out << message << std::flush;

#ifndef NDEBUG
        if (logFile.is_open()) { _log(message); }
#endif

    }
}

void Uci::log(const std::string& message) const {
    if (message.empty() || !logFile.is_open()) { return; }

    {
        ScopedLock lock{mutex};
        _log(message);
    }
}

void Uci::_log(const std::string& message) const {
    logFile << message << std::flush;

    // recover if the logFile is in a bad state
    if (!logFile) {
        logFile.clear();
        logFile.close();
        logFile.open(logFileName, std::ios::app);
        logFile << "#logFile recovered from a bad state\n" << std::flush;
    }
}
