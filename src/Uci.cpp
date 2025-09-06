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
    template <typename T>
    static T mebi(T bytes) { return bytes / (1024 * 1024); }

    template <typename T>
    static constexpr T permil(T n, T m) { return (n * 1000) / m; }
}

Uci::Uci(ostream &o) :
    root{*this},
    inputLine{std::string(1024, '\0')}, // preallocate 1024 bytes (~100 full moves)
    out{o},
    bestmove_(32, '\0')
{
    bestmove_.clear();
    ucinewgame();
}

void Uci::output(const std::string& message) const {
    {
        std::lock_guard<decltype(outMutex)> lock{outMutex};
        out << message << std::endl;
    }
#ifndef NDEBUG
    log(message);
#endif
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

#ifndef NDEBUG
        log('>' + currentLine);
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
    ob << root.limits;
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "uciok";
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

void Uci::ucinewgame() {
    root.newGame();
    root.setStartpos();
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
    root.limits.go(inputLine, root.sideOf(White));
    if (consume("searchmoves")) { root.limitMoves(inputLine); }

    mainSearchThread.start([this] {
        Node{root}.searchRoot();
        bestmove();
    });
}

void Uci::bestmove() {
    io::ostringstream o;
    o << "info"; nps(o) << root.pvScore << " pv" << root.pvMoves;
    output(o.str());

    o.str("");
    o << "bestmove " << root.pvMoves[0];
    if (root.limits.canPonder && root.pvMoves[1]) {
        o << " ponder " << root.pvMoves[1];
    }

    std::string sBestmove{o.str()};
    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (root.limits.waitBestmove()) {
            bestmove_ = sBestmove;
            return;
        }
    }

    output(sBestmove);
    root.limits.clear();
}

void Uci::stop() {
    root.limits.stop();

    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (!bestmove_.empty()) {
            output(bestmove_);
            bestmove_.clear();
            root.limits.clear();
            return;
        }
    }
}

void Uci::ponderhit() {
    root.limits.ponderhit();

    {
        std::lock_guard<decltype(bestmoveMutex)> lock{bestmoveMutex};
        if (!bestmove_.empty()) {
            output(bestmove_);
            bestmove_.clear();
            root.limits.clear();
            return;
        }
    }
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

void Uci::info_perft_bestmove() {
    Output ob{this};
    info_nps(ob);
    ob << "bestmove 0000";
    root.limits.clear();
}

void Uci::readyok() const {
    Output ob{this};
    info_fen(ob) << '\n';
    info_nps(ob);
    ob << "readyok";
}

ostream& Uci::nps(ostream& o) const {
    // avoid printing identical nps info in a row
    if (lastInfoNodes == root.nodeCounter) { return o; }
    lastInfoNodes = root.nodeCounter;

    o << " nodes " << lastInfoNodes;

    auto elapsedTime = root.limits.elapsedSinceStart();
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
    o << "info fen " << root;
    return o;
}

void Uci::info_iteration(Ply draft) const {
    Output ob{this};
    ob << "info depth " << draft; nps(ob);
}

void Uci::info_pv(Ply draft) const {
    Output ob{this};
    ob << "info depth " << draft; nps(ob) << root.pvScore << " pv" << root.pvMoves;
}

void Uci::info_perft_depth(Ply draft, node_count_t perft) const {
    Output ob{this};
    ob << "info depth " << draft << " perft " << perft; nps(ob);
}

void Uci::info_perft_currmove(int moveCount, const UciMove& currentMove, node_count_t perft) const {
    Output ob{this};
    ob << "info currmovenumber " << moveCount << " currmove " << currentMove << " perft " << perft; nps(ob);
}
