#include "UciSearchLimits.hpp"
#include "Position.hpp"

void UciSearchLimits::newSearch() {
    infinite_ = false;
    pondering_.store(false, std::memory_order_relaxed);
    stop_.store(false, std::memory_order_release);

    searchStartTime_ = timeNow();

    nodes_ = 0;
    nodesLimit_ = NodeCountMax;
    nodesQuota_ = 0;
    lastInfoNodes_ = NodeCountMax;

    time_ = {{ 0ms, 0ms }};
    inc_ = {{ 0ms, 0ms }};
    movetime_ = 0ms;
    movestogo_ = 0;

    timePool_ = UnlimitedTime;
    timeControl_ = ExactTime;
    easyMove_ = UciMove{};
    iterLowMaterialBonus_ = 0;

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
    assert (0 <= nodesQuota_);
    assert (nodesQuota_ < QuotaLimit);
    //assert (0 <= nodes);
    assert (nodes_ <= nodesLimit_);
    assert (static_cast<decltype(nodesLimit_)>(nodesQuota_) <= nodes_);
}

ReturnStatus UciSearchLimits::refreshQuota() const {
    assertNodesOk();

    if (hardDeadlineReached()) {
        return ReturnStatus::Stop;
    }

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
            assertNodesOk();
            return ReturnStatus::Stop;
        }
    }

    assert (0 < nodesQuota_ && nodesQuota_ <= QuotaLimit);
    nodes_ += nodesQuota_; // allocate new nodesQuota
    --nodesQuota_; //count current node

    assertNodesOk();
    return ReturnStatus::Continue;
}

template <UciSearchLimits::deadline_t Deadline>
bool UciSearchLimits::reachedTime() const {
    if (nodes_ == 0) { return false; } // skip checking before search even started
    if (stop_.load(std::memory_order_seq_cst)) { return true; }
    if (timePool_ == UnlimitedTime || pondering_.load(std::memory_order_relaxed)) { return false; }

    auto timePool = timePool_;
    if (timeControl_ != ExactTime) {
        int deadlineRatio = Deadline;
        if (Deadline == IterationDeadline) { deadlineRatio += iterLowMaterialBonus_; }

        timePool *= static_cast<int>(timeControl_) * deadlineRatio;
        timePool /= static_cast<int>(HardMove) * HardDeadline;
    }

    bool deadlineReached = timePool < elapsedSinceStart();
    return deadlineReached;
}
bool UciSearchLimits::hardDeadlineReached() const { return reachedTime<HardDeadline>(); }
bool UciSearchLimits::iterationDeadlineReached() const { return reachedTime<IterationDeadline>(); }

void UciSearchLimits::updateMoveComplexity(UciMove bestMove) const {
    if (timeControl_ == ExactTime) { return; }

    if (easyMove_.none()) {
        // Easy Move: root best move never changed
        easyMove_ = bestMove;
    } else if (easyMove_ != bestMove) {
        easyMove_ = bestMove;
        // Hard Move: root best move just have changed
        timeControl_ = HardMove;
    } else if (timeControl_ == HardMove) {
        // Normal Move: root best move have not changed during last two iterations
        timeControl_ = NormalMove;
    }
}

int UciSearchLimits::lookAheadMoves() const { return movestogo_ > 0 ? std::min(movestogo_, 16) : 16; }
TimeInterval UciSearchLimits::lookAheadTime(Side si) const { return time_[si] + inc_[si] * (lookAheadMoves() - 1); }
TimeInterval UciSearchLimits::averageMoveTime(Side si) const { return lookAheadTime(si) / lookAheadMoves(); }

void UciSearchLimits::setSearchDeadlines(const Position* p) {
    if (movetime_ > 0ms) {
        timePool_ = movetime_;
        timeControl_ = ExactTime;
        return;
    }

    auto noTimeLimits = time_[Side{My}] <= 0ms && inc_[Side{My}] <= 0ms;
    if (infinite_ || noTimeLimits) {
        timePool_ = UnlimitedTime;
        timeControl_ = ExactTime;
        return;
    }

    // [0..6] startpos = 6, queens exchanged = 4, R vs R endgame = 1
    int gamePhase = p ? p->gamePhase() : 4;
    iterLowMaterialBonus_ = 4 - std::clamp(gamePhase, 1, 5);

    // HardMove or HardDeadline may spend more than average move time
    auto optimumTime = averageMoveTime(Side{My}) + (canPonder_ ? averageMoveTime(Side{Op}) / 2 : 0ms);

    // allocate more time for the first out of book move in the game (fill up empty TT)
    if (isNewGame_) { optimumTime *= 13; optimumTime /= 8; isNewGame_ = false; }

    optimumTime *= static_cast<int>(HardMove) * HardDeadline;
    optimumTime /= static_cast<int>(NormalMove) * AverageTimeScale;

    auto maximumTime = lookAheadTime(Side{My});

    // can spend 2/8..4/4 of all remaining time (including future time increments)
    maximumTime *= 6 - std::clamp(gamePhase, 2, 4); // 2..4
    maximumTime /= 4 + (lookAheadMoves()+2)/4; // 4..8

    maximumTime = std::clamp(maximumTime, TimeInterval{0}, time_[Side{My}] * 63/64 - moveOverhead_);

    timePool_ = std::clamp(optimumTime - moveOverhead_, TimeInterval{0}, maximumTime);
    timeControl_ = EasyMove;
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

void UciSearchLimits::go(istream& is, Side white, const Position* p) {
    const Side black{~white};
    while (is >> std::ws, !is.eof()) {
        if      (io::consume(is, "depth"))    { is >> maxDepth_; }
        else if (io::consume(is, "nodes"))    { is >> nodesLimit_;  if (nodesLimit_  <= 0)  { nodesLimit_  = 0; } }
        else if (io::consume(is, "movetime")) { is >> movetime_;    if (movetime_    < 0ms) { movetime_    = 0ms; } }
        else if (io::consume(is, "wtime"))    { is >> time_[white]; if (time_[white] < 0ms) { time_[white] = 0ms; } }
        else if (io::consume(is, "btime"))    { is >> time_[black]; if (time_[black] < 0ms) { time_[black] = 0ms; } }
        else if (io::consume(is, "winc"))     { is >> inc_[white];  if (inc_[white]  < 0ms) { inc_[white]  = 0ms; }; }
        else if (io::consume(is, "binc"))     { is >> inc_[black];  if (inc_[black]  < 0ms) { inc_[black]  = 0ms; } }
        else if (io::consume(is, "movestogo")){ is >> movestogo_;   if (movestogo_   < 0)   { movestogo_   = 0; } }
        else if (io::consume(is, "mate"))     { is >> maxDepth_; maxDepth_ = Ply{std::abs(maxDepth_.v()) * 2 + 1}; } // TODO: implement mate in n moves
        else if (io::consume(is, "ponder"))   { pondering_.store(true, std::memory_order_relaxed); }
        else if (io::consume(is, "infinite")) { infinite_ =  true; }
        else { break; }
    }

    setSearchDeadlines(p);
}

void UciSearchLimits::setoption(istream& is) {
    if (io::consume(is, "Move Overhead")) {
        io::consume(is, "value");

        is >> moveOverhead_;
        if (moveOverhead_ < 0ms) { moveOverhead_ = UciSearchLimits::MoveOverheadDefault; }

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
    os << "option name Move Overhead type spin min 0 max 10000 default " << moveOverhead_ << '\n';
    os << "option name Ponder type check default " << (canPonder_ ? "true" : "false") << '\n';
    return os;
}

ostream& UciSearchLimits::nps(ostream& os) const {
    lastInfoNodes_ = getNodes();
    os << " nodes " << lastInfoNodes_;

    auto elapsedTime = elapsedSinceStart();
    if (elapsedTime >= 1ms) {
        os << " time " << elapsedTime << " nps " << ::nps(lastInfoNodes_, elapsedTime);
    }
    return os;
}

ostream& UciSearchLimits::info_nps(ostream& os) const {
    if (hasNewNodes()) {
        os << "info"; nps(os) << '\n';
    }
    return os;
}
