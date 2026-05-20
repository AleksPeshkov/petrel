#include "UciSearchLimits.hpp"
#include "Uci.hpp"

void UciSearchLimits::newSearch() {
    infinite_ = false;
    pondering_.store(false, std::memory_order_relaxed);
    stop_.store(false, std::memory_order_release);

    searchStartTime_ = timeNow();
    lastInfoTime_ = searchStartTime_;

    nodes_ = 0;
    nodesLimit_ = NodeCountMax;
    nodesQuota_ = 0;
    lastInfoNodes_ = 0;

    time_ = {{ 0ms, 0ms }};
    inc_ = {{ 0ms, 0ms }};
    movetime_ = 0ms;
    movestogo_ = 0;

    timePool_ = UnlimitedTime;
    lowMaterialQuotaBonus_ = 0;

    timeStrategy_ = ExactTime;
    lastMove_ = {};

    maxDepth_ = MaxPly;
}

void UciSearchLimits::stop() {
    infinite_ = false;
    pondering_.store(false, std::memory_order_relaxed);
    stop_.store(true, std::memory_order_release);
}

void UciSearchLimits::ponderhit() {
    pondering_.store(false, std::memory_order_relaxed);
}

void UciSearchLimits::assertNodesOk() const {
    assert (0 <= nodesQuota_); assert (nodesQuota_ < QuotaLimit);
    //assert (0 <= nodes);
    assert (nodes_ <= nodesLimit_);
    assert (static_cast<decltype(nodesLimit_)>(nodesQuota_) <= nodes_);
}

ReturnStatus UciSearchLimits::refreshQuota() const {
    assertNodesOk();

    // expected nodesQuata_ == 0
    nodes_ -= nodesQuota_;
    //nodesQuota_ = 0; // keeps invariant, but redundant

    auto nodesRemaining = nodesLimit_ - nodes_;
    if (nodesRemaining >= QuotaLimit) {
        nodesQuota_ = QuotaLimit;
    }
    else {
        nodesQuota_ = static_cast<decltype(nodesQuota_)>(nodesRemaining);
        if (nodesQuota_ == 0) {
            // `go nodes` limit reached
            assertNodesOk();
            return ReturnStatus::Stop;
        }
    }

    assert (0 < nodesQuota_); assert (nodesQuota_ <= QuotaLimit);
    nodes_ += nodesQuota_; // allocate new nodesQuota

    return lastDeadlineReached();
}

template <UciSearchLimits::time_quota_t TimeQuota>
ReturnStatus UciSearchLimits::reachedTime() const {
    if (stop_.load(std::memory_order_seq_cst)) { return ReturnStatus::Stop; } // unconditional stop
    if (timePool_ == UnlimitedTime || pondering_.load(std::memory_order_relaxed)) { return ReturnStatus::Continue; }
    if (getNodes() < QuotaLimit) { return ReturnStatus::Continue; } // avoid early time check throttling

    auto timePool = timePool_;
    if (timeStrategy_ != ExactTime) {
        int timeQuota = TimeQuota;
        if (TimeQuota != MaxQuota) { timeQuota += lowMaterialQuotaBonus_; }

        timePool *= +timeStrategy_ * timeQuota;
        timePool /= +HardMove * MaxQuota;
    }

    bool deadlineReached = timePool < elapsedSinceStart();
    return deadlineReached ? ReturnStatus::Stop : ReturnStatus::Continue;
}
ReturnStatus UciSearchLimits::lastDeadlineReached() const { return reachedTime<MaxQuota>(); }
ReturnStatus UciSearchLimits::iterationDeadlineReached() const { return reachedTime<IterationQuota>(); }

ReturnStatus UciSearchLimits::updateTimeStrategy(const PrincipalVariation& pv) const {
    if (timeStrategy_ == ExactTime) { return ReturnStatus::Continue; }

    auto bestMove = pv.move(0_ply);

    if (lastMove_.none()) {
        // Easy Move: root best move never changed
        lastMove_ = bestMove;
    } else if (lastMove_ != bestMove) {
        lastMove_ = bestMove;
        // Hard Move: root best move just have changed
        timeStrategy_ = HardMove;
    } else if (timeStrategy_ == HardMove) {
        // Normal Move: root best move have not changed during last two iterations
        timeStrategy_ = NormalMove;
    }

    // good place to check time as there are no wasted search nodes
    // and timeStrategy_ just possibly changed
    return lastDeadlineReached();
}

int UciSearchLimits::lookAheadMoves() const { return movestogo_ > 0 ? std::min(movestogo_, 16) : 16; }
TimeInterval UciSearchLimits::lookAheadTime(Side si) const { return time_[si] + inc_[si] * (lookAheadMoves() - 1); }
TimeInterval UciSearchLimits::averageMoveTime(Side si) const { return lookAheadTime(si) / lookAheadMoves(); }

void UciSearchLimits::setTimeDeadlines(const Position& position) {
    if (movetime_ > 0ms) {
        timePool_ = movetime_;
        timeStrategy_ = ExactTime;
        return;
    }

    auto noTimeLimits = time_[Side{My}] <= 0ms && inc_[Side{My}] <= 0ms;
    if (infinite_ || noTimeLimits) {
        timePool_ = UnlimitedTime;
        timeStrategy_ = ExactTime;
        return;
    }

    // [0..6] startpos = 6, queens exchanged = 4, R vs R endgame = 1
    int gamePhase = position.gamePhase();

    auto maximumTime = lookAheadTime(Side{My});
    {
        // board material left correction
        maximumTime *= 8 - std::clamp(gamePhase, 3, 5);
        maximumTime /= 4; // 75%, 100%, 125%

        if (lookAheadMoves() == 16) {
            maximumTime /= 4; // 25%
        } else {
            // solved for f(1) = 100%, f(2) = 75%, f(16) = 25%
            maximumTime *= 19 + lookAheadMoves();
            maximumTime /= 12 + 8*lookAheadMoves();
        }

        maximumTime = std::clamp(maximumTime, TimeInterval{0}, time_[Side{My}] - moveOverhead_);
    }

    auto optimumTime = averageMoveTime(Side{My}) + (canPonder_ ? averageMoveTime(Side{Op}) / 2 : 0ms);
    {
        // allocate 1.6x more time for the first out of book move in the game (fill up TT and history data)
        if (isNewGame_) { optimumTime *= 13; optimumTime /= 8; isNewGame_ = false; }

        // MaxQuota and/or HardMove time strategy may spend up to 5x more than average (optimium) move time
        optimumTime *= +MaxQuota * HardMove; // time * 512
        // average thinking time for OptimumTimeQuota with NormalMove time strategy
        optimumTime /= +OptimumTimeQuota * NormalMove; // time / 100
    }

    timePool_ = std::clamp(optimumTime - moveOverhead_, TimeInterval{0}, maximumTime);
    lowMaterialQuotaBonus_ = 4 - std::clamp(gamePhase, 1, 5);
    timeStrategy_ = EasyMove;
}

namespace {
    istream& operator >> (istream& is, TimeInterval& timeInterval) {
        int msecs;
        if (is >> msecs) {
            timeInterval = std::chrono::duration_cast<TimeInterval>(std::chrono::milliseconds{msecs} );
        }
        return is;
    }

    ostream& operator << (ostream& os, const TimeInterval& timeInterval) {
        return os << std::chrono::duration_cast<std::chrono::milliseconds>(timeInterval).count();
    }
}

void UciSearchLimits::go(istream& is, UciPosition& position) {
    const Side white{position.sideOf(White)};
    const Side black{~white};

    while (is >> std::ws, !is.eof()) {
        if      (io::consume(is, "wtime"))    { is >> time_[white]; if (time_[white] < 0ms) { time_[white] = 0ms; } }
        else if (io::consume(is, "btime"))    { is >> time_[black]; if (time_[black] < 0ms) { time_[black] = 0ms; } }
        else if (io::consume(is, "winc"))     { is >> inc_[white];  if (inc_[white]  < 0ms) { inc_[white]  = 0ms; } }
        else if (io::consume(is, "binc"))     { is >> inc_[black];  if (inc_[black]  < 0ms) { inc_[black]  = 0ms; } }
        else if (io::consume(is, "movetime")) { is >> movetime_;    if (movetime_    < 0ms) { movetime_    = 0ms; } }
        else if (io::consume(is, "nodes"))    { is >> nodesLimit_;  if (nodesLimit_  <= 0)  { nodesLimit_  = 0; } }
        else if (io::consume(is, "movestogo")){ is >> movestogo_;   if (movestogo_   < 0)   { movestogo_   = 0; } }
        else if (io::consume(is, "depth"))    { is >> maxDepth_; }
        else if (io::consume(is, "ponder"))   { pondering_.store(true, std::memory_order_relaxed); }
        else if (io::consume(is, "infinite")) { infinite_ =  true; }
        else if (io::consume(is, "searchmoves")) { position.limitMoves(is); break; }
        else { break; }
    }

    setTimeDeadlines(position);
}

void UciSearchLimits::setoption(istream& is) {
    if (io::consume(is, "Move Overhead")) {
        io::consume(is, "value");

        TimeInterval moveOverhead{0};
        is >> moveOverhead;
        moveOverhead_ = std::max(moveOverhead, MoveOverheadDefault);

        if (!is) { io::fail_rewind(is); }
        return;
    }

    if (io::consume(is, "Ponder")) {
        io::consume(is, "value");

        if (io::consume(is, "true"))  { canPonder_ = true; return; }
        if (io::consume(is, "false")) { canPonder_ = false; return; }

        io::fail_rewind(is);
        return;
    }
}

ostream& UciSearchLimits::uciok(ostream& os) const {
    os << "\noption name Move Overhead type spin min " << MoveOverheadDefault << " max 10000 default " << moveOverhead_;
    os << "\noption name Ponder type check default " << (canPonder_ ? "true" : "false");
    return os;
}

ostream& UciSearchLimits::average_nps(ostream& os) const {
    auto nodes = getNodes();
    auto time = ::timeNow();
    auto elapsedTime = time - searchStartTime_;

    os << " nodes " << nodes;
    if (elapsedTime >= 1ms) {
        os << " time " << elapsedTime << " nps " << ::nps(nodes, elapsedTime);
    }

    lastInfoNodes_ = nodes;
    lastInfoTime_ = time;
    return os;
}

ostream& UciSearchLimits::instant_nps(ostream& os) const {
    auto nodes = getNodes();
    auto time = ::timeNow();
    auto elapsedTime = time - searchStartTime_;

    auto deltaNodes = nodes - lastInfoNodes_;
    auto deltaTime = time - lastInfoTime_;

    os << " nodes " << nodes;
    if (elapsedTime >= 1ms) {
        os << " time " << elapsedTime << " nps " << ::nps(deltaNodes, deltaTime);
    }

    lastInfoNodes_ = nodes;
    lastInfoTime_ = time;
    return os;
}
