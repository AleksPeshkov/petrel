#ifndef NODE_ROOT_HPP
#define NODE_ROOT_HPP

#include "chrono.hpp"
#include "typedefs.hpp"
#include "Node.hpp"
#include "Repetitions.hpp"
#include "Tt.hpp"

class HistoryMoves {
    using _t = Side::arrayOf< PieceType::arrayOf<Square::arrayOf<Move>> >;
    _t v;
public:
    void clear() { std::memset(&v, 0, sizeof(v)); }
    const Move& operator() (Color c, PieceType ty, Square sq) const { return v[c][ty][sq]; }
    void set(Color c, PieceType ty, Square sq, const Move& move) { v[c][ty][sq] = move; }
};

// triangular array
class CACHE_ALIGN PvMoves {
    Ply::arrayOf< Ply::arrayOf<UciMove> > pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void set(Ply ply, UciMove move) {
        pv[ply][0] = move;
        auto target = &pv[ply][1];
        auto source = &pv[ply+1][0];
        while ((*target++ = *source++));
        pv[ply+1][0] = {};
    }

    operator const UciMove* () const { return &pv[0][0]; }

    friend ostream& operator << (ostream& out, const PvMoves& pvMoves) {
        return out << &pvMoves.pv[0][0];
    }
};

class Uci;

// position extended with repetition history
class NodeRoot : public Node {
public:
    Tt tt;
    Repetitions repetitions;
    PvMoves pvMoves;
    Score pvScore = NoScore;
    HistoryMoves counterMove;

    Uci& uci;

protected:
    Color colorToMove_ = White; //root position side to move color

public:
    NodeRoot(Uci& u) : Node{*this}, uci{u} {}

    constexpr Side sideOf(Color color) const { return colorToMove_.is(color) ? My : Op; }
    constexpr Color colorToMove(Ply d = {0}) const { return colorToMove_ << d; }

    void newGame() {
        tt.newGame();
        counterMove.clear();
        newSearch();
    }

    void newSearch() {
        tt.newSearch();
        pvMoves.clear();
        pvScore = NoScore;
    }

    void newIteration() {
        tt.newIteration();
    }

    void setHash(size_t bytes) { tt.setSize(bytes); }
};

#endif
