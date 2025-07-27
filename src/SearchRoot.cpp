#include <mutex>
#include "SearchRoot.hpp"
#include "NodeAbRoot.hpp"
#include "NodePerftRoot.hpp"
#include "UciGoLimit.hpp"

namespace {
    io::ostream& operator << (io::ostream& out, TimeInterval& timeInterval) {
        using namespace std::chrono_literals;
        if (timeInterval < 1ms) { return out; }

        return out << " time " << duration_cast<Msecs>(timeInterval).count();
    }

    template <typename T>
    static T mebi(T bytes) { return bytes / (1024 * 1024); }

    template <typename T>
    static constexpr T permil(T n, T m) { return (n * 1000) / m; }

    template<class BasicLockable>
    class OutputBuffer : public std::ostringstream {
        io::ostream& out;
        BasicLockable& lock;
        typedef std::lock_guard<decltype(lock)> Guard;
    public:
        OutputBuffer (io::ostream& o, BasicLockable& l) : std::ostringstream{}, out(o), lock(l) {}
        ~OutputBuffer () { Guard g{lock}; out << str() << std::flush; }
    };
}

#define OUTPUT(ob) OutputBuffer<decltype(outLock)> ob(out, outLock)

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

#define OUTPUT(ob) OutputBuffer<decltype(outLock)> ob(out, outLock)

void SearchRoot::uciok() const {
    bool isChess960 = position.isChess960();

    OUTPUT(ob);
    ob << "id name petrel\n";
    ob << "id author Aleks Peshkov\n";
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "option name Hash type spin min 0 max " << ::mebi(tt.getMaxSize()) << " default " << ::mebi(tt.memory.getSize()) << '\n';
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

void SearchRoot::bestmove() const {
    OUTPUT(ob);
    info_nps(ob);
    ob << "bestmove " << pvMoves[0] << '\n';
}

void SearchRoot::infoIterationEnd(Ply draft) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob) << '\n';
}

void SearchRoot::infoNewPv(Ply draft, Score score) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob) << score << " pv" << pvMoves << '\n';
}

void SearchRoot::perft_depth(Ply draft, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info depth " << draft << " perft " << perft; nps(ob) << '\n';
}

void SearchRoot::perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info currmovenumber " << moveCount << " currmove " << currentMove << " perft " << perft;
    nps(ob) << '\n';
}

void SearchRoot::perft_finish() const {
    OUTPUT(ob);
    info_nps(ob);
    ob << "bestmove 0000\n";
}

#undef OUTPUT

void SearchRoot::setHash(size_t bytes) {
    tt.memory.allocate(bytes); tt.memory.zeroFill();
}

ostream& SearchRoot::nps(ostream& o) const {
    node_count_t nodes = nodeCounter;
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
        o << " hhitratio " << ::permil(tt.counter.hits, tt.counter.reads);
    }
    return o;
}

ostream& SearchRoot::info_nps(ostream& o) const {
    std::ostringstream buffer;
    nps(buffer);

    if (!buffer.str().empty()) {
        o << "info" << buffer.str() << '\n';
    }
    return o;
}

NodeControl NodeCounter::count(const SearchRoot& root) {
    assertOk();

    if (nodesQuota == 0) {
        return refreshQuota(root);
    }

    assert (nodesQuota > 0);
    --nodesQuota;

    assertOk();
    return NodeControl::Continue;
}

NodeControl NodeCounter::refreshQuota(const SearchRoot& root) {
    assertOk();
    assert (nodesQuota == 0);
    //nodes -= nodesQuota;

    auto nodesRemaining = nodesLimit - nodes;
    if (nodesRemaining >= QuotaLimit) {
        nodesQuota = QuotaLimit;
    }
    else {
        nodesQuota = static_cast<decltype(nodesQuota)>(nodesRemaining);
        if (nodesQuota == 0) {
            assertOk();
            return NodeControl::Abort;
        }
    }

    if (root.isStopped()) {
        nodesLimit = nodes;
        nodesQuota = 0;

        assertOk();
        return NodeControl::Abort;
    }

    assert (0 < nodesQuota && nodesQuota <= QuotaLimit);
    nodes += nodesQuota;
    --nodesQuota; //count current node
    assertOk();

    //inform UCI that search is responsive
    root.readyok();

    return NodeControl::Continue;
}
