#include "UciGoRoot.hpp"
#include "UciGoLimit.hpp"
#include "NodeAbRoot.hpp"
#include "NodePerftRoot.hpp"

template <typename T>
static T mebi(T bytes) { return bytes / (1024 * 1024); }

void UciGoRoot::setHash(size_t bytes) {
    tt.setSize(bytes);
}

void UciGoRoot::newGame() {
    tt.newGame();
    newSearch();
}

void UciGoRoot::newSearch() {
    tt.newSearch();
    counterMove.clear();
    pvMoves.clear();
    lastInfoNodes = 0;
    fromSearchStart = {};
}

void UciGoRoot::go(const UciGoLimit& limit) {
    newSearch();
    nodeCounter = { limit.nodes };

    auto searchId = searchThread.start(static_cast<std::unique_ptr<Node>>(
        std::make_unique<NodeAbRoot>(limit, static_cast<SearchRoot&>(*this))
    ));

    if (!limit.isInfinite) {
        timer.start(limit.getThinkingTime(), searchThread, searchId);
    }
}

void UciGoRoot::goPerft(Ply depth, bool isDivide) {
    newSearch();
    nodeCounter = {};

    searchThread.start(static_cast<std::unique_ptr<Node>>(
        std::make_unique<NodePerftRoot>(position, static_cast<SearchRoot&>(*this), depth, isDivide)
    ));
}

void UciGoRoot::uciok() const {
    bool isChess960 = position.isChess960();

    OUTPUT(ob);
    ob << "id name petrel\n";
    ob << "id author Aleks Peshkov\n";
    ob << "option name UCI_Chess960 type check default " << (isChess960 ? "true" : "false") << '\n';
    ob << "option name Hash type spin"
       << " min "     << ::mebi(tt.minSize())
       << " max "     << ::mebi(tt.maxSize())
       << " default " << ::mebi(tt.size())
       << '\n';
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
