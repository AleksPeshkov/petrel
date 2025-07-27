#include "SearchRoot.hpp"
#include "NodeAbRoot.hpp"
#include "NodePerftRoot.hpp"
#include "UciGoLimit.hpp"
#include "OutputBuffer.hpp"

#define OUTPUT(ob) OutputBuffer<decltype(outLock)> ob(out, outLock)

namespace {
    io::ostream& operator << (io::ostream& out, TimeInterval& timeInterval) {
        using namespace std::chrono_literals;
        if (timeInterval < 1ms) { return out; }

        return out << " time " << duration_cast<Msecs>(timeInterval).count();
    }
}

void SearchRoot::newGame() {
    tt.memory.zeroFill();
    newSearch();
}

void SearchRoot::newSearch() {
    pvMoves.clear();
    tt.counter ={0,0,0};
    lastInfoNodes = 0;
    fromSearchStart = {};
}

void SearchRoot::newIteration() {
    tt.nextAge();
}

void SearchRoot::go(const UciGoLimit& limit) {
    newSearch();
    nodeCounter = { limit.nodes };

    auto searchId = searchThread.start(static_cast<std::unique_ptr<Node>>(
        std::make_unique<NodeAbRoot>(limit, *this)
    ));

    if (!limit.isInfinite) {
        timer.start(limit.getThinkingTime(), searchThread, searchId);
    }
}

void SearchRoot::goPerft(Ply depth, bool isDivide) {
    newSearch();
    nodeCounter = {};

    searchThread.start(static_cast<std::unique_ptr<Node>>(
        std::make_unique<NodePerftRoot>(position, *this, depth, isDivide)
    ));
}

NodeControl SearchRoot::countNode() {
    return nodeCounter.count(*this);
}

void SearchRoot::uciok() const {
    bool isChess960 = position.isChess960();

    OUTPUT(ob);
    ob << "id name petrel\n";
    ob << "id author Aleks Peshkov\n";
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "option name Hash type spin min 0 max " << mebi(tt.getMaxSize()) << " default " << mebi(tt.memory.getSize()) << '\n';
    ob << "uciok\n";
}

void SearchRoot::isready() const {
    OUTPUT(ob);
    if (!isBusy()) {
        isreadyWaiting = false;
        ob << "readyok\n";
    }
    else {
        isreadyWaiting = true;
    }
}

void SearchRoot::infoPosition() const {
    OUTPUT(ob);
    ob << "info fen " << position << '\n';
    ob << "info" << position.evaluate() << '\n';
}

void SearchRoot::readyok() const {
    if (isreadyWaiting) {
        std::ostringstream ob;
        info_nps(ob, nodeCounter);
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

void SearchRoot::bestmove() const {
    OUTPUT(ob);
    if (lastInfoNodes != nodeCounter) {
        ob << "info"; nps(ob, nodeCounter) << '\n';
    }
    ob << "bestmove " << pvMoves[0] << '\n';
}

void SearchRoot::infoIterationEnd(Ply draft) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob, nodeCounter) << '\n';
}

void SearchRoot::infoNewPv(Ply draft, Score score) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob, nodeCounter) << score << " pv" << pvMoves << '\n';
}

void SearchRoot::perft_depth(Ply draft, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info depth " << draft << " perft " << perft; nps(ob, nodeCounter) << '\n';
}

void SearchRoot::perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info currmovenumber " << moveCount << " currmove " << currentMove << " perft " << perft; nps(ob, nodeCounter) << '\n';
}

void SearchRoot::perft_finish() const {
    if (lastInfoNodes != nodeCounter) {
        OUTPUT(ob);
        ob << "info"; nps(ob, nodeCounter) << '\n';
    }
    OUTPUT(ob);
    ob << "bestmove 0000\n";
}

void SearchRoot::setHash(size_t bytes) {
    tt.memory.allocate(bytes); tt.memory.zeroFill();
}

ostream& SearchRoot::nps(ostream& o, node_count_t nodes) const {
    if (lastInfoNodes == nodes) {
        return o;
    }
    lastInfoNodes = nodes;

    auto timeInterval = fromSearchStart.getDuration();

    o << " nodes " << nodes << timeInterval << " nps " << ::nps(nodes, timeInterval);

    if (tt.counter.reads > 0) {
        o << " hwrites " << tt.counter.writes;
        o << " hhits " << tt.counter.hits;
        o << " hreads " << tt.counter.reads;
        o << " hhitratio " << permil(tt.counter.hits, tt.counter.reads);
    }
    return o;
}

ostream& SearchRoot::info_nps(ostream& o, node_count_t nodes) const {
    std::ostringstream buffer;
    nps(buffer, nodes);

    if (!buffer.str().empty()) {
        o << "info" << buffer.str() << '\n';
    }
    return o;
}

#undef OUTPUT
