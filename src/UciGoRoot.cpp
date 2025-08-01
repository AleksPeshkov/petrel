#include "UciGoRoot.hpp"
#include "UciGoLimit.hpp"
#include "Search.hpp"
#include "SearchPerft.hpp"

template <typename T>
static T mebi(T bytes) { return bytes / (1024 * 1024); }

void UciGoRoot::newGame() {
    tt.memory.zeroFill();
    newSearch();
}

void UciGoRoot::newSearch() {
    counterMove.clear();
    pvMoves.clear();
    tt.counter ={0,0,0};
    lastInfoNodes = 0;
    fromSearchStart = {};
}

void UciGoRoot::go(const UciGoLimit& limit) {
    newSearch();
    nodeCounter = { limit.nodes };

    auto id = searchThread.start([this, depth = limit.depth]() {
        NodeAb{position, *this}.visitRoot(depth);
    });

    if (!limit.isInfinite) {
        searchThread.startTimer(id, limit.getThinkingTime());
    }
}

void UciGoRoot::goPerft(Ply depth, bool isDivide) {
    newSearch();
    nodeCounter = {};

    searchThread.start([this, depth, isDivide]() {
        PerftNode rootNode{position, *this, depth};
        rootNode.visitRoot(isDivide);
    });
}

void UciGoRoot::setHash(size_t bytes) {
    tt.memory.allocate(bytes); tt.memory.zeroFill();
}

void UciGoRoot::uciok() const {
    bool isChess960 = position.isChess960();

    OUTPUT(ob);
    ob << "id name petrel\n";
    ob << "id author Aleks Peshkov\n";
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "option name Hash type spin min 0 max " << ::mebi(tt.getMaxSize()) << " default " << ::mebi(tt.memory.getSize()) << '\n';
    ob << "uciok\n";
}

void UciGoRoot::isready() const {
    OUTPUT(ob);
    if (!isBusy()) {
        isreadyWaiting = false;
        ob << "readyok\n";
    }
    else {
        isreadyWaiting = true;
    }
}

void UciGoRoot::infoPosition() const {
    OUTPUT(ob);
    ob << "info fen " << position << '\n';
    ob << "info" << position.evaluate() << '\n';
}
