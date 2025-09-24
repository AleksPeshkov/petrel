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
    killer2{grandParent ? grandParent->killer2 : Move{}}
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
        d = Ply{d > 0 ? d-1 : 0};
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
    child->alpha = -alpha - 1;
    assert (child->beta == -alpha);
    child->isPv = false;
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
        killer2 = killer1;
        killer1 = newKiller;
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

    if (draft == 0 && !inCheck()) {
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

    // cannot capture the king, so do not even try
    RETURN_CUTOFF (goodCaptures(child, OP.pieces() - PiMask{TheKing}));

    canBeKiller = true;

    Pi lastPi = TheKing;
    Bb newMoves = {};

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

        // try quiet moves of the last moved piece (unless it was captured)
        {
            Square from = parent->movedPieceTo();
            if (MY.bbSide().has(from)) {
                // last moved piece
                lastPi = MY.pieceAt(from);

                // new moves of the last moved piece
                newMoves = movesOf(lastPi);

                if (from != parent->movedPieceFrom()) {
                    // unless it was a pawn promotion move
                    newMoves %= parent->OP.attacksOf(lastPi);
                }

                // try new safe moves of the last moved piece
                for (Square to : newMoves % bbAttacked()) {
                    RETURN_CUTOFF (child->searchMove({from, to}));
                }

                // keep unsafe news moves for later
                newMoves &= bbAttacked();
            }
        }

        // new safe quiet moves, except for the last moved piece (or king)
        for (Pi pi : MY.pieces() - lastPi) {
            Square from = MY.squareOf(pi);
            for (Square to : movesOf(pi) % parent->OP.attacksOf(pi) % bbAttacked()) {
                RETURN_CUTOFF (child->searchMove({from, to}));
            }
        }
    }

    // all the rest safe quiet moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);
        for (Square to : movesOf(pi) % bbAttacked()) {
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    if (newMoves.any()) {
        Square from = parent->movedPieceTo();
        Pi pi = MY.pieceAt(from);

        // unsafe new moves of the last moved piece
        for (Square to : newMoves) {
            RETURN_CUTOFF (child->searchIfLegal({from, to}));
        }

        // the rest moves of the last moved piece
        for (Square to : movesOf(pi)) {
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    // remaining (bad) captures and all underpromotions
    RETURN_CUTOFF (badCaptures(child, OP.pieces() - PiMask{TheKing}));

    // all the rest (quiet) moves, LVA order
    auto pieces = MY.pieces();
    while (pieces.any()) {
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
    //TODO: create lighter quiescence search node
    Node node{this};
    const auto child = &node;

    // king cannot be captured, so do not even try
    return goodCaptures(child, OP.pieces() - PiMask{TheKing});
}

ReturnStatus Node::goodCaptures(Node* child, const PiMask& victims) {
    canBeKiller = false;
    if (MY.promotables().any()) {
        // queen promotions with capture, always good
        for (Pi victim : OP.pieces() & OP.piecesOn(Rank8)) {
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

        // seems too rare to bother, but enpassant attacker does not attack victim pawn square
        // that makes following test of attacker singularity broken
        if (OP.isEnPassant(victim)) {
            for (Pi attacker : attackers & MY.enPassantPawns()) {
                Square from = MY.squareOf(attacker);
                RETURN_CUTOFF (child->searchMove({from, to}));
            }
            attackers %= MY.enPassantPawns();
            if (attackers.none()) { continue; }
        }

        PieceType victimType = OP.typeOf(victim);

        // simple SEE function, checks only two cases:
        // 1) prune as bad capture if solo attacker tries to capture defended lower valued victim
        // 2) prune as bad capture if lower valued victim defended by a pawn
        // the rest of uncertain captures considered good enough to seek in QS

        // complete attackers set, including already searched captures (see note about enpassant capture)
        PiMask allAttackers = MY.attackersTo(to);
        if (allAttackers.isSingleton()) {
            Pi attacker = allAttackers.index();

            if (bbAttacked().has(to)) {
                // singleton attacker and victim defended
                //TODO: check X-Ray attacks
                if (!MY.isLessOrEqualValue(attacker, victimType)) {
                    // skip bad capture of defended victim
                    //TODO: check if bad capture makes discovered check
                    continue;
                }
            }

            Square from = MY.squareOf(attacker);
            RETURN_CUTOFF (child->searchMove({from, to}));
            continue;
        }

        if (OP.bbPawnAttacks().has(~to)) {
            // victim is protected by at least one pawn
            // try only winning or equal captures
            //TODO: check if defending pawn is pinned
            //TODO: check if bad capture(s) makes discovered check
            attackers &= MY.lessOrEqualValue(victimType);
        }

        //TODO: more complex SEE
        //TODO: try killer heuristics for uncertain and bad captures

        while (attackers.any()) {
            // LVA (least valuable attacker) order
            Pi attacker = attackers.leastValuable(); attackers -= attacker;

            Square from = MY.squareOf(attacker);
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::badCaptures(Node* child, const PiMask& victims) {
    // MVV (most valuable victim)
    for (Pi victim : victims) {
        Square to = ~OP.squareOf(victim);

        // exclude underpromotions because of illegal phantom captures
        PiMask attackers = canMoveTo(to) % MY.promotables();
        while (attackers.any()) {
            // LVA (least valuable attacker)
            Pi attacker = attackers.leastValuable(); attackers -= attacker;

            Square from = MY.squareOf(attacker);
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    // unsorted (under)promotions with or without capture
    for (Pi pawn : MY.promotables()) {
        Square from = MY.squareOf(pawn);
        for (Square to : movesOf(pawn)) {
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
