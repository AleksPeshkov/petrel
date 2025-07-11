#include "SearchControl.hpp"
#include "NodePerftRoot.hpp"
#include "NodeAbRoot.hpp"
#include "UciSearchInfo.hpp"
#include "SearchLimit.hpp"

SearchControl::SearchControl (UciSearchInfo& i) : info{i}, pvMoves{} {}

void SearchControl::newGame() {
    transpositionTable.clear();
    newSearch();
}

void SearchControl::newSearch() {
    pvMoves.clear();
    newIteration();
    transpositionTable.clearCounter();
    info.clear();
}

void SearchControl::newIteration() {
    transpositionTable.nextAge();
}

void SearchControl::go(const SearchLimit& limit) {
    newSearch();

    nodeCounter = NodeCounter(limit.nodes);
    auto duration = limit.getThinkingTime();

    auto searchId = searchThread.start(
        limit.isPerft
            ? (limit.depth
                ? (limit.isDivide
                    ? static_cast<std::unique_ptr<Node>>(std::make_unique<NodePerftDivideDepth>(limit, *this))
                    : static_cast<std::unique_ptr<Node>>(std::make_unique<NodePerftRootDepth>(limit, *this))
                )
                : (limit.isDivide
                    ? static_cast<std::unique_ptr<Node>>(std::make_unique<NodePerftDivideIterative>(limit, *this))
                    : static_cast<std::unique_ptr<Node>>(std::make_unique<NodePerftRootIterative>(limit, *this))
                )
            )
            : static_cast<std::unique_ptr<Node>>(std::make_unique<NodeAbRoot>(limit, *this))
    );

    timer.start(duration, searchThread, searchId);
}

NodeControl SearchControl::countNode() {
    return nodeCounter.count(*this);
}

const Move* SearchControl::getPv(ply_t ply) const {
    return &pvMoves[ply];
}

void SearchControl::setPv(ply_t ply, const Move& move) {
    pvMoves(ply, move);
}

void SearchControl::uciok() const {
    info.uciok(transpositionTable.getInfo());
}

void SearchControl::isready() const {
    info.isready(!isBusy());
}

void SearchControl::infoPosition() const {
    info.position();
}

void SearchControl::readyok() const{
    info.readyok(nodeCounter, transpositionTable);
}

void SearchControl::bestmove(Score score) const {
    info.bestmove(pvMoves, score, nodeCounter, transpositionTable);
}

void SearchControl::infoDepth(ply_t draft, Score score) const {
    info.report_depth(draft, pvMoves, score, nodeCounter, transpositionTable);
}

void SearchControl::perft_depth(ply_t draft, node_count_t perft) const {
    info.perft_depth(draft, perft, nodeCounter, transpositionTable);
}

void SearchControl::perft_currmove(index_t moveCount, const Move& currentMove, node_count_t perft) const {
    info.perft_currmove(currentMove, moveCount, perft, nodeCounter, transpositionTable);
}

void SearchControl::perft_finish() const {
    info.perft_finish(nodeCounter, transpositionTable);
}

void SearchControl::setHash(size_t bytes) {
    transpositionTable.resize(bytes);
}
