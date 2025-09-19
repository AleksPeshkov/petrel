#include "Node.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; \
    if (status == ReturnStatus::Stop) { return ReturnStatus::Stop; } \
    if (status == ReturnStatus::BetaCutoff) { return ReturnStatus::BetaCutoff; }} ((void)0)

TtSlot::TtSlot (Z z, Move move, Score score, Bound b, Ply d) : s{
    move.from(),
    move.to(),
    score,
    b,
    static_cast<unsigned>(d),
    static_cast<unsigned>(z >> DataBits)
} {}

TtSlot::TtSlot (const Node* n, Bound b) : TtSlot{
    n->zobrist(),
    n->currentMove,
    n->score.toTt(n->ply),
    b,
    n->draft
} {}

Node::Node (const PositionMoves& p, Uci& r) :
    PositionMoves{p}, parent{nullptr}, grandParent{nullptr}, root{r} {}

Node::Node (Node* n) :
    PositionMoves{}, parent{n}, grandParent{n->parent}, root{n->root},
    ply{n->ply + 1}, draft{n->draft > 0 ? n->draft-1 : 0},
    alpha{-n->beta}, beta{-n->alpha}, isPv(n->isPv),
    killer1{grandParent ? grandParent->killer1 : Move{}},
    killer2{grandParent ? grandParent->killer2 : Move{}},
    killer3{grandParent ? grandParent->killer3 : Move{}}
{}

ReturnStatus Node::searchRoot() {
    root.newSearch();

    auto rootMovesClone = moves();
    repMask = root.repetitions.repMask(colorToMove());
    origin = root.tt.prefetch<TtSlot>(zobrist());

    if (root.limits.isIterationDeadline()) {
        // we have no time to search, return TT move immediately if found
        ++root.tt.reads;
        ttSlot = *origin;
        isHit = (ttSlot == zobrist());
        if (!isHit) {
            io::log("#no time, no TT record found");
        } else {
            Move ttMove = {ttSlot};
            if (!isLegalMove(ttMove)) {
                if (Move{ttSlot}) {
                    io::log("#no time, illegal TT move");
                } else {
                    io::log("#no time, TT null move found");
                }
            } else {
                if (root.limits.canPonder) {
                    Node node{this};
                    const auto child = &node;

                    child->makeMove(ttMove);
                    ++root.tt.reads;
                    child->ttSlot = *child->origin;
                    child->isHit = (child->ttSlot == child->zobrist());
                    if (child->isHit) {
                        Move ttMove2 = {child->ttSlot};
                        if (ttMove2) {
                            child->generateMoves();
                            if (child->isLegalMove(ttMove2)) {
                                ++root.tt.hits;
                                root.pvMoves.set(1, child->uciMove(ttMove2));
                            }
                        }
                    }
                }

                ++root.tt.hits;
                root.pvScore = ttSlot.score(ply);
                root.pvMoves.set(0, uciMove(ttMove));
                io::log("#no time, return move from TT");
                return ReturnStatus::Stop;
            }
        }
    }

    for (draft = {1}; draft <= root.limits.depth; ++draft) {
        setMoves(rootMovesClone);
        alpha = MinusInfinity;
        beta = PlusInfinity;
        auto status = search();

        root.newIteration();
        refreshTtPv();

        RETURN_IF_STOP (status);

        root.info_iteration(draft);

        if (root.limits.isIterationDeadline()) { return ReturnStatus::Stop; }
    }

    return ReturnStatus::Continue;
}

 // refresh PV in TT before new search iteration if it was occasionally overwritten
 void Node::refreshTtPv() {
    Position pos{root.position_};
    Score s = root.pvScore;
    Ply d = draft;

    const Move* pv = root.pvMoves;
    for (Move move; (move = *pv++);) {
        auto o = root.tt.addr<TtSlot>(pos.zobrist());
        *o = TtSlot{pos.zobrist(), move, s, ExactScore, d};
        ++root.tt.writes;

        //we cannot use makeZobrist() because of en passant legality validation
        pos.makeMove(move.from(), move.to());
        s = -s;
        d = d > 0 ? d-1 : 0;
    }
}

void Node::makeMove(Move move) {
    Square from = move.from();
    Square to = move.to();

    parent->currentMove = move;
    parent->clearMove(from, to);
    Position::makeMove(parent, from, to);
    origin = root.tt.prefetch<TtSlot>(zobrist());
}

ReturnStatus Node::searchMove(Move move) {
    RETURN_IF_STOP (root.limits.countNode());
    makeMove(move);

    if (rule50() < 2) { repMask = RepetitionMask{}; }
    else if (grandParent) { repMask = RepetitionMask{grandParent->repMask, grandParent->zobrist()}; }
    else { repMask = root.repetitions.repMask(colorToMove()); }

    root.pvMoves.set(ply, UciMove{});
    return parent->negamax(this);
}

ReturnStatus Node::negamax(Node* child) const {
    child->generateMoves();
    RETURN_IF_STOP (child->search());

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    auto childScore = -child->score;

    if (beta <= childScore) {
        score = childScore;
        failHigh();
        return ReturnStatus::BetaCutoff;
    }

    if (alpha < childScore) {
        assert (isPv); // alpha < childScore < beta, so current window cannot be zero
        if (!child->isPv) {
            // Principal Variation Search:
            // zero window search failed high, research with full window
            child->alpha = -beta;
            assert (child->beta == -alpha);
            child->isPv = true;
            assert (child->alpha < child->beta-1);
            return negamax(child);
        }

        score = childScore;
        alphaImproved = true;
        alpha = childScore;
        child->beta = -alpha;
        updatePv();
    } else if (score < childScore) {
        score = childScore;
    }

    if (ply == 0 && root.limits.isRootMoveDeadline()) {
        return ReturnStatus::Stop;
    }

    // set window for the next move search
    assert (child->beta == -alpha);
    child->alpha = child->beta-1;
    child->isPv = false;
    child->draft = draft > 0 ? draft-1 : 0;
    return ReturnStatus::Continue;
}

void Node::failHigh() const {
    if (parent && canBeKiller) { parent->updateKillerMove(currentMove); }
    *origin = TtSlot{this, FailHigh};
    ++root.tt.writes;
}

void Node::updateKillerMove(Move newKiller) const {
    assert (newKiller);

    if (killer1 != newKiller) {
        if (killer3 == newKiller) {
            killer3 = killer2;
        }
        killer2 = killer1;
        killer1 = newKiller;
    }

    if (grandParent && grandParent->killer1 != newKiller && grandParent->killer2 != newKiller) {
        grandParent->killer3 = newKiller;
    }

    if (currentMove) {
        root.counterMove.set(colorToMove(),  MY.typeAt(currentMove.from()), currentMove.to(), newKiller);
    }
}

void Node::updatePv() const {
    root.pvMoves.set(ply, uciMove(currentMove));
    *origin = TtSlot{this, ExactScore};
    ++root.tt.writes;

    if (ply == 0) {
        root.pvScore = score;
        root.info_pv(draft);
    }
}

ReturnStatus Node::search() {
    // mate-distance pruning
    if (ply >= 1) {
        alpha = std::max(alpha, Score::checkmated(ply));
        if (alpha >= beta) { score = alpha; return ReturnStatus::BetaCutoff; }
    }

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    if (moves().none()) {
        // checkmate or stalemate
        score = inCheck() ? Score::checkmated(ply) : Score{DrawScore};
        return ReturnStatus::Continue;
    }

    if (ply >= 1 && (rule50().isDraw() || isRepetition() || isDrawMaterial())) {
        score = DrawScore;
        return ReturnStatus::Continue;
    }

    // check extension
    if (ply >= 1 && inCheck()) {
        draft = draft + 1;
    }

    if (draft == 0) {
        return quiescence();
    }

    ++root.tt.reads;
    ttSlot = *origin;
    isHit = (ttSlot == zobrist());
    if (isHit) {
        if (Move{ttSlot} && !isLegalMove(Move{ttSlot})) {
            io::log("#illegal TT move");
            isHit = false;
        } else {
            ++root.tt.hits;

            if (ttSlot.draft() >= draft && !isPv) {
                Bound bound = ttSlot;
                Score ttScore = ttSlot.score(ply);

                if ((bound & FailHigh) && beta <= ttScore) {
                    score = ttScore;
                    return ReturnStatus::BetaCutoff;
                } else if ((bound & FailLow) && ttScore <= alpha) {
                    score = ttScore;
                    return ReturnStatus::Continue;
                }
            }
        }
    }

    if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
        return ReturnStatus::Continue;
    }

    // prepare empty child node to make moves into
    Node node{this};
    const auto child = &node;

    canBeKiller = false;
    score = NoScore;

    if (isHit && Move{ttSlot}) {
        RETURN_CUTOFF (child->searchMove(ttSlot));
    }

    // impossible to capture the king, do not even try to save time
    RETURN_CUTOFF (goodCaptures(child, OP.pieces() - PiMask{TheKing}));

    canBeKiller = true;

    if (parent) {
        // primary killer move, updated by previous siblings
        RETURN_CUTOFF (child->searchIfLegal(parent->killer1));

        // countermove heuristic: refutation of the last opponent's move
        Move opMove = parent->currentMove;
        RETURN_CUTOFF (child->searchIfLegal( root.counterMove(
            parent->colorToMove(), parent->MY.typeAt(opMove.from()), opMove.to()
        ) ));

        // secondary killer move, backup of previous primary killer
        RETURN_CUTOFF (child->searchIfLegal(parent->killer2));

        // repeated killer heuristic (can change while searching descendants of previous killer3)
        while (isLegalMove(parent->killer3)) {
            RETURN_CUTOFF (child->searchMove(parent->killer3));
        }
    }

    // going to search only non-captures, mask out remaining unsafe captures to avoid redundant safety checks
    //TRICK: ~ is not a negate bitwise operation but byteswap -- flip opponent's bitboard
    Bb badSquares = ~(OP.bbPawnAttacks() | OP.bbSide());
    PiMask safePieces = {};

    // quiet moves from unsafe to safe squares
    // skip pawns to avoid wasting time on safety check as pawns comes first in the final loop anyway
    // castling move is a rook move and picked early (safety check of castling rook is not very exact, but who cares?)
    //TODO: make king ordered last in default (MV first) piece order
    for (Pi pi : MY.pieces() - MY.pawns()) {
        Square from = MY.squareOf(pi);

        if (!bbAttacked().has(from)) {
            // not attacked piece, postpone search of its moves
            // do not need more precise safety check here
            safePieces += pi;
            continue;
        }

        RETURN_CUTOFF (goodNonCaptures(child, pi, movesOf(pi) % badSquares));
    }

    while (safePieces.any()) {
        Pi pi = safePieces.leastValuable(); safePieces -= pi;
        RETURN_CUTOFF (goodNonCaptures(child, pi, movesOf(pi) % badSquares));
    }

    // all remaining unsorted moves, starting with pawns, all king moves are last
    for (PiMask pieces = MY.pieces(); pieces.any(); ) {
        Pi pi = pieces.leastValuable(); pieces -= pi;

        Square from = MY.squareOf(pi);
        for (Square to : movesOf(pi)) {
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    if (!alphaImproved) {
        // fail low, currenMove used to write into TT
        currentMove = isHit && Move{ttSlot} ? Move{ttSlot} : Move{}; // pass the previous TT move
        *origin = TtSlot(this, FailLow);
        ++root.tt.writes;
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::goodNonCaptures(Node* child, Pi pi, Bb moves) {
    Square from = MY.squareOf(pi);
    PieceType ty = MY.typeOf(pi);

    for (Square to : moves) {
        if (bbAttacked().has(to)) {
            Pi defender = OP.attackersTo(~to).leastValuable();
            if (OP.isLessValue(defender, ty)) {
                // skip move if square defended by less valued piece
                continue;
            }
            if (!MY.bbPawnAttacks().has(to) && (MY.attackersTo(to).popcount()) <= OP.attackersTo(~to).popcount()) {
                // skip move at defended square if nobody helps to attack it
                continue;
            }
        }
        assert (!OP.bbSide().has(~to));
        RETURN_CUTOFF (child->searchMove({from, to}));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck());

    // stand pat
    score = evaluate();
    if (beta <= score) {
        return ReturnStatus::BetaCutoff;
    }
    if (ply == MaxPly) {
        // no room to search deeper
        return ReturnStatus::Continue;
    }
    if (alpha < score) {
        alpha = score;
    }

    // prepare empty child node to make moves into
    //TODO: create lighter quiescence node without zobrist hashing and repetition detection
    Node node{this};
    const auto child = &node;

    // impossible to capture the king, do not even try to save time
    return goodCaptures(child, OP.pieces() - PiMask{TheKing});
}

ReturnStatus Node::goodCaptures(Node* child, const PiMask& victims) {
    canBeKiller = false;
    if (MY.promotables().any()) {
        // queen promotions with capture, always good
        for (Pi victim : OP.pieces() & OP.piecesOn(Rank1)) {
            Square to = ~OP.squareOf(victim);
            for (Pi attacker : canMoveTo(to) & MY.promotables()) {
                Square from = MY.squareOf(attacker);
                RETURN_CUTOFF (child->searchMove({from, to}));
            }
        }

        // queen promotions without capture
        for (Pi pawn : MY.promotables()) {
            Square from = MY.squareOf(pawn);
            Square to{File{from}, Rank8};
            RETURN_CUTOFF (child->searchIfLegal({from, to}));
        }
    }

    // MVV (most valuable victim) order
    for (Pi victim : victims) {
        Square to = ~OP.squareOf(victim);

        // exclude underpromotions, should be no queen promotions anymore
        PiMask attackers = canMoveTo(to) % MY.promotables();
        if (attackers.none()) { continue; }

        // simple SEE function, checks only two cases:
        // 1) victim defended by at least one pawn
        // 2) attackers does not outnumber defenders (not precise, but effective condition)
        // then prune captures by more valuable attackers
        // the rest uncertain captures considered good enough to seek in QS
        //TODO: check for X-Ray attackers and defenders
        //TODO: check if bad capture makes discovered check
        //TODO: check if defending pawn is pinned and cannot recapture
        //TODO: try killer heuristics for uncertain and bad captures
        //TODO: try more complex and precise SEE
        if (OP.bbPawnAttacks().has(~to) || MY.attackersTo(to).popcount() <= OP.attackersTo(~to).popcount()) {
            attackers &= MY.lessOrEqualValue(OP.typeOf(victim));
        }

        while (attackers.any()) {
            // LVA (least valuable attacker) order
            Pi attacker = attackers.leastValuable(); attackers -= attacker;

            Square from = MY.squareOf(attacker);
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    return ReturnStatus::Continue;
}

UciMove Node::uciMove(Move move) const {
    Square from{move.from()}; Square to{move.to()};
    return UciMove{from, to, isSpecial(from, to), colorToMove(), root.chessVariant()};
}

constexpr Color Node::colorToMove() const {
    return root.colorToMove() << ply;
}

// insufficient mate material
bool Node::isDrawMaterial() const {
    auto& my = MY.evaluation();
    auto& op = OP.evaluation();

    if (my.hasMatingPieces() || op.hasMatingPieces()) { return false; }

    // here both sides can have only minors pieces
    auto myMinors = my.count(Knight) + my.count(Bishop);
    auto opMinors = op.count(Knight) + op.count(Bishop);

    // lone minors cannot mate
    if (myMinors <= 1 && opMinors <= 1) { return true; }

    if (myMinors == 2) {
        if (my.count(Bishop) == 0 && opMinors <= 1) { return true; }
        if (my.count(Bishop) == 1 && opMinors == 1) { return true; }
        if (my.count(Bishop) == 2 && op.count(Bishop) == 1) { return true; }
    }

    if (opMinors == 2) {
        if (op.count(Bishop) == 0 && opMinors <= 1) { return true; }
        if (op.count(Bishop) == 1 && myMinors == 1) { return true; }
        if (op.count(Bishop) == 2 && my.count(Bishop) == 1) { return true; }
    }

    return false;
}

bool Node::isRepetition() const {
    if (rule50() < 4) { return false; }

    auto& z = zobrist();

    if (grandParent) {
        auto next = grandParent;
        while ((next = next->grandParent)) {
            if (next->zobrist() == z) {
                return true;
            }
            if (!next->repMask.has(z)) {
                return false;
            }
        }
    }

    return root.repetitions.has(colorToMove(), z);
}
