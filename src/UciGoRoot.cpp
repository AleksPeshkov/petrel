#include "UciGoRoot.hpp"
#include "UciGoLimit.hpp"
#include "Search.hpp"
#include "SearchPerft.hpp"

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

    auto searchId = searchThread.start(std::make_unique<SearchThread>(static_cast<SearchRoot&>(*this), limit));

    if (!limit.isInfinite) {
        timer.start(limit.getThinkingTime(), searchThread, searchId);
    }
}

void UciGoRoot::goPerft(Ply depth, bool isDivide) {
    newSearch();
    nodeCounter = {};

    searchThread.start(std::make_unique<PerftThread>(static_cast<SearchRoot&>(*this), depth, isDivide));
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
