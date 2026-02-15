#include "UciSearchLimits.hpp"
#include "Position.hpp"

void UciSearchLimits::newSearch() {
    searchStartTime_ = timeNow();

    nodes_ = 0;
    nodesLimit_ = NodeCountMax;
    nodesQuota_ = 0;
    lastInfoNodes_ = 0;

    infinite_ = false;
    timeout_.store(false, std::memory_order_seq_cst);
    pondering_.store(false, std::memory_order_release);

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
    timeout_.store(true, std::memory_order_seq_cst);
    pondering_.store(false, std::memory_order_release);
}

void UciSearchLimits::ponderhit() {
    pondering_.store(false, std::memory_order_release);
}

void UciSearchLimits::assertNodesOk() const {
    assert (0 <= nodesQuota_);
    assert (nodesQuota_ < QuotaLimit);
    //assert (0 <= nodes);
    assert (nodes_ <= nodesLimit_);
    assert (static_cast<decltype(nodesLimit_)>(nodesQuota_) <= nodes_);
}

ReturnStatus UciSearchLimits::countNode() const {
    assertNodesOk();

    if (nodesQuota_ == 0 || isStopped()) {
        return refreshQuota();
    }

    assert (nodesQuota_ > 0);
    --nodesQuota_;

    assertNodesOk();
    return ReturnStatus::Continue;
}

ReturnStatus UciSearchLimits::refreshQuota() const {
    assertNodesOk();
    nodes_ -= nodesQuota_;

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

    if (hardDeadlineReached()) {
        return ReturnStatus::Stop;
    }

    assert (0 < nodesQuota_ && nodesQuota_ <= QuotaLimit);
    nodes_ += nodesQuota_;
    --nodesQuota_; //count current node

    assertNodesOk();
    return ReturnStatus::Continue;
}

template <UciSearchLimits::deadline_t Deadline>
bool UciSearchLimits::reachedTime() const {
    if (isStopped()) { return true; }
    if (nodes_ == 0) { return false; } // skip checking before search even started
    if (Deadline != HardDeadline && timeControl_ == ExactTime) { return false; }
    if (timePool_ == UnlimitedTime || pondering_.load(std::memory_order_acquire)) { return false; }

    auto timePool = timePool_;
    if (timeControl_ != ExactTime) {
        int deadlineRatio = Deadline;
        if (Deadline == IterationDeadline) { deadlineRatio += iterLowMaterialBonus_; }

        timePool *= static_cast<int>(timeControl_) * deadlineRatio;
        timePool /= static_cast<int>(HardMove) * HardDeadline;
    }

    bool isDeadlineReached = timePool < elapsedSinceStart();
    if (isDeadlineReached) {
        nodesLimit_ = nodes_;
        nodesQuota_ = 0;
        assertNodesOk();
        timeout_.store(true, std::memory_order_seq_cst);
    }
    return isDeadlineReached;
}
bool UciSearchLimits::hardDeadlineReached() const { return reachedTime<HardDeadline>(); }
bool UciSearchLimits::iterationDeadlineReached() const { return reachedTime<IterationDeadline>(); }

void UciSearchLimits::updateMoveComplexity(UciMove bestMove) const {
    if (timeControl_ == ExactTime) { return; }

    if (!easyMove_) {
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
    optimumTime -= moveOverhead_;

    // can spend totalRatio/8 of all remaining time (including future time increments)
    auto totalRatio = 6 - std::clamp(gamePhase, 2, 4);

    auto maximumTime = lookAheadTime(Side{My}) * totalRatio / 8;
    maximumTime = std::min(time_[Side{My}] * 63/64, maximumTime);
    maximumTime = std::max(TimeInterval{0}, maximumTime - moveOverhead_);

    timePool_ = std::clamp(optimumTime, TimeInterval{0}, maximumTime);
    timeControl_ = EasyMove;
}

namespace {
    istream& operator >> (istream& in, TimeInterval& timeInterval) {
        int msecs;
        if (in >> msecs) {
            timeInterval = std::chrono::duration_cast<TimeInterval>(std::chrono::milliseconds{msecs} );
        }
        return in;
    }

    ostream& operator << (ostream& out, const TimeInterval& timeInterval) {
        return out << std::chrono::duration_cast<std::chrono::milliseconds>(timeInterval).count();
    }
}

void UciSearchLimits::go(istream& in, Side white, const Position* p) {
    const Side black{~white};
    while (in >> std::ws, !in.eof()) {
        if      (io::consume(in, "depth"))    { in >> maxDepth_;    if (maxDepth_    < 0)   { maxDepth_    = 0_ply; } }
        else if (io::consume(in, "nodes"))    { in >> nodesLimit_;  if (nodesLimit_  <= 0)  { nodesLimit_  = 0; } }
        else if (io::consume(in, "movetime")) { in >> movetime_;    if (movetime_    < 0ms) { movetime_    = 0ms; } }
        else if (io::consume(in, "wtime"))    { in >> time_[white]; if (time_[white] < 0ms) { time_[white] = 0ms; } }
        else if (io::consume(in, "btime"))    { in >> time_[black]; if (time_[black] < 0ms) { time_[black] = 0ms; } }
        else if (io::consume(in, "winc"))     { in >> inc_[white];  if (inc_[white]  < 0ms) { inc_[white]  = 0ms; }; }
        else if (io::consume(in, "binc"))     { in >> inc_[black];  if (inc_[black]  < 0ms) { inc_[black]  = 0ms; } }
        else if (io::consume(in, "movestogo")){ in >> movestogo_;   if (movestogo_   < 0)   { movestogo_   = 0; } }
        else if (io::consume(in, "mate"))     { in >> maxDepth_; maxDepth_ = Ply{std::abs(maxDepth_) * 2 + 1}; } // TODO: implement mate in n moves
        else if (io::consume(in, "ponder"))   { pondering_.store(true, std::memory_order_release); }
        else if (io::consume(in, "infinite")) { infinite_ =  true; }
        else { break; }
    }

    setSearchDeadlines(p);
}

void UciSearchLimits::setoption(istream& in) {
    if (io::consume(in, "Move Overhead")) {
        io::consume(in, "value");

        in >> moveOverhead_;
        if (moveOverhead_ < 0ms) { moveOverhead_ = UciSearchLimits::MoveOverheadDefault; }

        if (!in) { io::fail_rewind(in); }
        return;
    }

    if (io::consume(in, "Ponder")) {
        io::consume(in, "value");

        if (io::consume(in, "true"))  { canPonder_ = true; return; }
        if (io::consume(in, "false")) { canPonder_ = false; return; }

        io::fail_rewind(in);
        return;
    }
}

ostream& UciSearchLimits::uciok(ostream& out) const {
    out << "option name Move Overhead type spin min 0 max 10000 default " << moveOverhead_ << '\n';
    out << "option name Ponder type check default " << (canPonder_ ? "true" : "false") << '\n';
    return out;
}

ostream& UciSearchLimits::info_nps(ostream& out) const {
    lastInfoNodes_ = getNodes();
    out << "info nodes " << lastInfoNodes_;

    auto elapsedTime = elapsedSinceStart();
    if (elapsedTime >= 1ms) {
        out << " time " << elapsedTime << " nps " << ::nps(lastInfoNodes_, elapsedTime);
    }
    return out;
}
