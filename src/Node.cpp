#include "Node.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus returnStatus = visitor; \
    if (returnStatus == ReturnStatus::Stop) { return ReturnStatus::Stop; } \
    if (returnStatus == ReturnStatus::BetaCutoff) { return ReturnStatus::BetaCutoff; }} ((void)0)

TtSlot::TtSlot (Z z, Move move, Score score, Bound bound, Ply draft, bool canBeKiller) : s{
    move.from(),
    move.to(),
    score,
    bound,
    static_cast<unsigned>(draft),
    canBeKiller,
    static_cast<unsigned>(z >> DataBits)
} {}

TtSlot::TtSlot (const Node* n) : TtSlot{
    n->zobrist(),
    n->currentMove,
    n->score.toTt(n->ply),
    n->bound,
    n->draft,
    n->canBeKiller
} {}

Node::Node (const PositionMoves& p, const Uci& r) : PositionMoves{p}, root{r} {}

Node::Node (const Node* p) :
    PositionMoves{}, root{p->root}, parent{p}, grandParent{p->parent},
    ply{p->ply + 1}, draft{p->draft > 0 ? p->draft-1 : 0},
    alpha{-p->beta}, beta{-p->alpha}, isPv(p->isPv),
    pvIndex{p->pvIndex+1},
    killer1{grandParent ? grandParent->killer1 : Move{}},
    killer2{grandParent ? grandParent->killer2 : Move{}}
{}

ReturnStatus Node::searchRoot() {
    auto rootMovesClone = moves();
    repetitionHash = root.repetitions.repetitionHash(colorToMove());
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

                    child->makeMove(ttMove.from(), ttMove.to());
                    ++root.tt.reads;
                    child->ttSlot = *child->origin;
                    child->isHit = (child->ttSlot == child->zobrist());
                    if (child->isHit) {
                        Move ttMove2 = {child->ttSlot};
                        if (ttMove2) {
                            child->generateMoves();
                            if (child->isLegalMove(ttMove2)) {
                                ++root.tt.hits;
                                root.pvMoves.clearPly(PvMoves::Index{child->pvIndex+1});
                                root.pvMoves.set(child->pvIndex, child->uciMove(ttMove2), PvMoves::Index{child->pvIndex+1});
                            }
                        }
                    }
                }

                ++root.tt.hits;
                root.pvScore = ttSlot.score(ply);
                root.pvMoves.set(pvIndex, uciMove(ttMove), PvMoves::Index{pvIndex+1});
                io::log("#no time, return move from TT");
                return ReturnStatus::Stop;
            }
        }
    }

    for (draft = 1; draft <= root.limits.depth; ++draft) {
        setMoves(rootMovesClone);
        alpha = MinusInfinity;
        beta = PlusInfinity;
        auto returnStatus = search();

        root.newIteration();
        refreshTtPv();

        RETURN_IF_STOP (returnStatus);

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
        *o = TtSlot{pos.zobrist(), move, s, ExactScore, d, false};
        ++root.tt.writes;

        //we cannot use makeZobrist() because of en passant legality validation
        pos.makeMove(move.from(), move.to());
        s = -s;
        d = d-1;
    }
}

void Node::makeMove(Square from, Square to) {
    Position::makeMove(parent, from, to);
    origin = root.tt.prefetch<TtSlot>(zobrist());
    root.pvMoves.clearPly(pvIndex);
}

ReturnStatus Node::searchMove(Move move) {
    RETURN_IF_STOP (root.limits.countNode());

    Square from = move.from();
    Square to = move.to();
    parent->clearMove(from, to);
    parent->currentMove = move;
    makeMove(from, to);

    if (rule50() < 2) { repetitionHash = RepetitionHash{}; }
    else if (grandParent) { repetitionHash = RepetitionHash{grandParent->repetitionHash, grandParent->zobrist()}; }
    else { repetitionHash = root.repetitions.repetitionHash(colorToMove()); }

    return parent->negamax(this);
}

ReturnStatus Node::negamax(Node* child) const {
    child->generateMoves();
    RETURN_IF_STOP (child->search());

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    auto childScore = -child->score;

    if (childScore <= alpha) {
        if (score < childScore) {
            score = childScore;
        }
    } else {
        if (beta <= childScore) {
            score = childScore;
            failHigh();
            return ReturnStatus::BetaCutoff;
        }

        assert (alpha < childScore && childScore < beta);
        assert (isPv); // alpha < childScore < beta, so current window cannot be zero

        if (!child->isPv) {
            // Principal Variation Search (PVS) research with full window
            child->isPv = true;
            child->alpha = -beta;
            assert (child->beta == -alpha);
            assert (child->alpha < child->beta-1);
            return negamax(child);
        }

        score = childScore;
        alpha = childScore;
        child->beta = -alpha;
        updatePv(child);
    }

    if (ply == 0 && root.limits.isRootMoveDeadline()) {
        return ReturnStatus::Stop;
    }

    // set zero window for the next sibling move search
    assert (child->beta == -alpha);
    child->alpha = child->beta-1;
    child->isPv = false;
    return ReturnStatus::Continue;
}

void Node::failHigh() const {
    bound = FailHigh;
    *origin = TtSlot{this};
    ++root.tt.writes;

    if (parent && canBeKiller) {
        assert (currentMove);
        parent->updateKillerMove(currentMove);
    }
}

void Node::updateKillerMove(Move newKiller) const {
    if (killer1 != newKiller) {
        killer2 = killer1;
        killer1 = newKiller;
    }

    if (currentMove) {
        root.counterMove.set(colorToMove(),  MY.typeAt(currentMove.from()), currentMove.to(), newKiller);
    }
}

void Node::updatePv(Node* child) const {
    child->pvIndex = root.pvMoves.set(pvIndex, uciMove(currentMove), child->pvIndex);

    bound = ExactScore;
    *origin = TtSlot{this};
    ++root.tt.writes;

    if (parent && canBeKiller) {
        assert (currentMove);
        parent->updateKillerMove(currentMove);
    }

    if (ply == 0) {
        root.pvScore = score;
        root.info_pv(draft);
    }
}

ReturnStatus Node::search() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    score = NoScore;
    currentMove = {};
    bound = FailLow;

    if (moves().none()) {
        // checkmate or stalemate
        score = inCheck() ? Score::checkmated(ply) : Score{DrawScore};
        assert (!currentMove);
        return ReturnStatus::Continue;
    }

    if (ply >= 1) {
        // mate-distance pruning
        alpha = std::max(alpha, Score::checkmated(ply));
        if (!(alpha < beta)) {
            score = alpha;
            assert (!currentMove);
            return ReturnStatus::BetaCutoff;
        }

        if (rule50().isDraw() || isRepetition() || isDrawMaterial()) {
            score = DrawScore;
            assert (!currentMove);
            return ReturnStatus::Continue;
        }

        if (draft == 0 && !inCheck()) {
            return quiescence();
        }
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
                Bound ttBound = ttSlot;
                Score ttScore = ttSlot.score(ply);

                //TODO: refresh TT record if age is old
                if ((ttBound & FailHigh) && beta <= ttScore) {
                    score = ttScore;
                    currentMove = Move{ttSlot};
                    return ReturnStatus::BetaCutoff;
                } else if ((ttBound & FailLow) && ttScore <= alpha) {
                    score = ttScore;
                    currentMove = Move{ttSlot};
                    return ReturnStatus::Continue;
                }
            }
        }
    }

    if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
        assert (!currentMove);
        return ReturnStatus::Continue;
    }

    // prepare empty child node to make moves into
    Node node{this};
    const auto child = &node;

    if (isHit && Move{ttSlot}) {
        canBeKiller = ttSlot.canBeKiller();
        RETURN_CUTOFF (child->searchMove(ttSlot));
    }

    // impossible to capture the king, do not even try to save time
    canBeKiller = false;
    RETURN_CUTOFF (goodCaptures(child, OP.notKing()));
    canBeKiller = !inCheck();

    if (parent && !inCheck()) {
        // primary killer move, updated by previous siblings
        RETURN_CUTOFF (child->searchIfLegal(parent->killer1));

        // countermove heuristic: refutation of the last opponent's move
        Move opMove = parent->currentMove;
        RETURN_CUTOFF (child->searchIfLegal( root.counterMove(
            parent->colorToMove(), parent->MY.typeAt(opMove.from()), opMove.to()
        ) ));

        // secondary killer move, backup of previous primary killer
        RETURN_CUTOFF (child->searchIfLegal(parent->killer2));
    }

    // going to search only non-captures, mask out remaining unsafe captures to avoid redundant safety checks
    //TRICK: ~ is not a negate bitwise operation but byteswap -- flip opponent's bitboard
    Bb badSquares = ~(OP.bbPawnAttacks() | OP.bbSide());

    // safe (good) quiet non-pawn, non-king moves
    // skip king moves because they ordered first, safe anyway and rarely best moves in chess
    // skip pawns to avoid wasting time on safety check as pawns comes first in the final loop anyway
    // castling move is a rook move and picked early (safety check of castling rook is not very exact, but who cares?)
    //TODO: make king ordered last in default (MV first) piece order
    for (Pi pi : MY.figures()) {
        Square from = MY.squareOf(pi);
        PiMask opLessValue = OP.lessValue(MY.typeOf(pi));

        for (Square to : movesOf(pi) % badSquares) {
            if (bbAttacked().has(to)) {
                if ((OP.attackersTo(~to) & opLessValue).any()) {
                    // skip move if square defended by less valued piece
                    continue;
                }
                if ((MY.attackersTo(to) % PiMask{pi}).none()) {
                    // skip move to defended square if nobody else attacks it
                    continue;
                }
            }
            assert (!OP.bbSide().has(~to));
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    // all remaining unsorted moves, starting with pawns, all king moves are last
    for (PiMask pieces = MY.pieces(); pieces.any(); ) {
        Pi pi = pieces.leastValuable(); pieces -= pi;

        Square from = MY.squareOf(pi);
        for (Square to : movesOf(pi)) {
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    if (bound == FailLow) {
        // fail low, no good move found, write back previous TT move if any
        currentMove = isHit ? Move{ttSlot} : Move{};
        *origin = TtSlot(this);
        ++root.tt.writes;
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck());

    // stand pat
    score = evaluate();
    if (beta <= score) {
        assert (!currentMove);
        return ReturnStatus::BetaCutoff;
    }
    if (ply == MaxPly) {
        // no room to search deeper
        assert (!currentMove);
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
    return goodCaptures(child, OP.notKing());
}

ReturnStatus Node::goodCaptures(Node* child, PiMask victims) {
    if (MY.promotables().any()) {
        // queen promotions with capture, always good
        for (Pi victim : OP.pieces() & OP.piecesOn(Rank{Rank1})) {
            Square to = ~OP.squareOf(victim);
            for (Pi pi : canMoveTo(to) & MY.promotables()) {
                Square from = MY.squareOf(pi);
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
        if (OP.bbPawnAttacks().has(~to) || OP.attackersTo(~to).popcount() >= MY.attackersTo(to).popcount()) {
            attackers &= MY.lessOrEqualValue(OP.typeOf(victim));
        }

        while (attackers.any()) {
            // LVA (least valuable attacker) order
            Pi pi = attackers.leastValuable(); attackers -= pi;

            Square from = MY.squareOf(pi);
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    return ReturnStatus::Continue;
}

UciMove Node::uciMove(Move move) const {
    Square from = move.from();
    Square to = move.to();
    return UciMove{from, to, isSpecial(from, to), colorToMove(), root.chessVariant()};
}

constexpr Color Node::colorToMove() const {
    return Color{root.colorToMove() << ply};
}

// insufficient mate material
bool Node::isDrawMaterial() const {
    auto& my = MY.evaluation();
    auto& op = OP.evaluation();

    if (my.hasMatingPieces() || op.hasMatingPieces()) { return false; }

    auto myBishops = my.count(Bishop);
    auto opBishops = op.count(Bishop);

    // here both sides can have only minors pieces
    auto myMinors = my.count(Knight) + myBishops;
    auto opMinors = op.count(Knight) + opBishops;

    if (myMinors >= 2 || opMinors >= 2) { return false; }

    // lone minor cannot mate
    if (myMinors <= 1 && opMinors <= 1) { return true; }

    // myMinors:opMinors == 2:1 | 1:2 | 2:0 | 0:2
    if (myMinors == 2) {
        if (myBishops == 0) { return true; }
        if (myBishops == 1 && opMinors == 1) { return true; }
        if (myBishops == 2 && opBishops == 1) { return true; }
    } else {
        assert (opMinors == 2);
        if (opBishops == 0) { return true; }
        if (opBishops == 1 && myMinors == 1) { return true; }
        if (opBishops == 2 && myBishops == 1) { return true; }
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
            if (!next->repetitionHash.has(z)) {
                return false;
            }
        }
    }

    return root.repetitions.has(colorToMove(), z);
}
