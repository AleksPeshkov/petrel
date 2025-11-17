#ifndef UCI_HPP
#define UCI_HPP

#include <fstream>
#include <mutex>
#include "io.hpp"
#include "Repetitions.hpp"
#include "Thread.hpp"
#include "Tt.hpp"
#include "UciPosition.hpp"
#include "UciMove.hpp"
#include "UciSearchLimits.hpp"

template<int _Size>
class HistoryMoves {
    constexpr static int N = _Size;
    using Index = ::Index<N>;
    using _t = Side::arrayOf< PieceType::arrayOf<Square::arrayOf< typename Index:: template arrayOf<Move> >> >;
    _t v;

public:
    static constexpr int Size = N;

    void clear() {
        std::memset(&v, 0, sizeof(v)); //TRICK: Move{} == int16_t{0}
    }

    constexpr const Move& get(int i, Color c, PieceType ty, Square sq) const {
        return v[c][ty][sq][Index{i}];
    }

    constexpr void set(Color c, PieceType ty, Square sq, Move move) {
        auto& slot = v[c][ty][sq];

        // if present inside, move it to front
        for (int i = 0; i < N-1; ++i) {
            if (slot[i] == move) {
                // shift by one, put move at front
                for (int j = i; j > 0; --j) {
                    slot[j] = slot[j-1];
                }
                slot[0] = move;
                return;
            }
        }

        // not found side: shift all down, insert at front
        for (int i = N-1; i > 0; --i) {
            slot[i] = slot[i-1];
        }
        slot[0] = move;;
    }
};

// triangular array
class CACHE_ALIGN PvMoves {
    static constexpr auto triangularArraySize = (Ply::Last+1) * (Ply::Last+2) / 2;
public:
    using Index = ::Index<triangularArraySize>;
    Index::arrayOf<UciMove> pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void clearPly(Index i) { pv[i] = UciMove{}; }

    Index set(Index parent, UciMove move, Index child) {
        pv[parent++] = move;
        assert (parent <= child);
        while ((pv[parent++] = pv[child++]));
        pv[parent] = UciMove{};
        return parent; // new child index
    }

    operator const UciMove* () const { return &pv[0]; }

    friend ostream& operator << (ostream& out, const PvMoves& pvMoves) {
        return out << static_cast<const UciMove*>(pvMoves);
    }
};

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
public:
    UciPosition position_; // result of parsing 'position' command
    UciSearchLimits limits; // result of parsing 'go' command

    mutable Tt tt;
    Repetitions repetitions;
    mutable PvMoves pvMoves;
    mutable Score pvScore = NoScore;
    mutable HistoryMoves<4> counterMove;
    mutable HistoryMoves<4> followMove;

private:
    Thread mainSearchThread;

    // stream buffer for parsing current input line
    io::istringstream inputLine;

    // output stream
    ostream& out;
    mutable std::mutex outMutex;

    // if we are in infinite or ponder mode, we cannot send bestmove immediately
    std::string bestmove_;
    std::mutex bestmoveMutex;

    // avoid race conditions betweeen Uci output and main search thread output

    // avoid printing identical 'info nps' lines in a row
    mutable node_count_t lastInfoNodes = 0;

    std::string logFileName; // no log by default
    mutable std::ofstream logFile;
    mutable std::mutex logMutex;

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // something left unparsed
    bool hasMoreInput() { return io::hasMore(inputLine); }

    //UCI command handlers

    void uciok() const;
    void setoption();
    void setHash();
    void ucinewgame();
    void position();

    void readyok() const;
    void go();
    void stop();
    void ponderhit();
    void bestmove();

    void goPerft();
    void info_perft_bestmove();

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
    ostream& info_fen(ostream&) const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and to log file
    void output(const std::string&) const;

    // log messages to the logFile named by logFileName
    void log(const std::string&) const;

    constexpr ChessVariant chessVariant() const { return position_.chessVariant(); }
    constexpr Color colorToMove(Ply ply = Ply{0}) const { return position_.colorToMove(ply); }

    void newGame() {
        tt.newGame();
        counterMove.clear();
        followMove.clear();
        newSearch();
    }

    void newSearch() {
        tt.newSearch();
        pvMoves.clear();
        pvScore = NoScore;
    }

    void newIteration() const {
        tt.newIteration();
    }

    void setHash(size_t bytes) { tt.setSize(bytes); }
    void info_pv(Ply) const;
    void info_iteration(Ply) const;

    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(int moveCount, const UciMove& currentMove, node_count_t) const;
};

#endif
