#ifndef SCORE_HPP
#define SCORE_HPP

#include "Index.hpp"

// search tree distance in halfmoves
struct Ply : Index<Ply, 64> {
    constexpr explicit Ply(_t n) : Index{n > 0 ? n : 0} { assertOk(); }
    friend constexpr Ply operator""_ply(unsigned long long);
    friend constexpr Ply operator + (Ply a, Ply b) { return Ply{a.v_ + b.v_}; }
    friend constexpr Ply operator - (Ply a, Ply b) { return Ply{a.v_ - b.v_}; }
    friend constexpr Ply operator * (Ply a, int n) { return Ply{a.v_ * n}; }
    friend constexpr Ply operator / (Ply a, int n) { return Ply{a.v_ / n}; }

    friend ostream& operator << (ostream& os, Ply ply) { return os << ply.v_; }

    friend istream& operator >> (istream& is, Ply& ply) {
        _t n;
        auto before = is.tellg();
        is >> n;
        if (!(0 <= n && n <= Last)) { return io::fail_pos(is, before); }
        ply = Ply{n};
        return is;
    }
};
constexpr Ply MaxPly{Ply::Last}; // Ply is limited to [0 .. MaxPly]
constexpr Ply operator""_ply(unsigned long long n) { return Ply{static_cast<Ply::_t>(n)}; }

// color to move of the given ply
constexpr Color::_t distance(Color c, Ply ply) { return static_cast<Color::_t>((ply.v() ^ static_cast<unsigned>(c.v())) & Color::Mask); }

enum Bound : u8_t {
    NoBound = 0,
    FailLow = 0b01, // upper bound
    FailHigh = 0b10, // lower bound
    ExactScore = FailLow | FailHigh,
    BoundMask = ExactScore
};

constexpr Bound operator ~ (Bound bound) {
    return (bound == FailLow) ? FailHigh : (bound == FailHigh) ? FailLow : bound;
}

static constexpr int ScoreBitSize = 14;

// position evaluation score, fits in 14 bits
enum score_enum : i16_t {
    NoScore = -singleton(ScoreBitSize-1), //-8192 TRICK: assume two's complement
    MinusInfinity = NoScore + 1, // no position should eval to it

    MateLoss = MinusInfinity + 1, // -8190, mated in 0, only even negative values for mated positions

    // ... negative mate range of scores (loss) ...

    MinEval = MateLoss + Ply::Size, // -8126, minimal (negative) non mate score

    // ... negative position static evaluation range of scores ...

    DrawScore = 0,

    //... positive position static evalutation range of scores ...

    MaxEval = -MinEval, // 8126, maximal (positive) non mate score bound for a position

    // ... positive mate range of scores (win) ...

    MateWin = -MateLoss, // 8190, mate in 0 (impossible), only odd positive values for positions winned with mate

    PlusInfinity = -MinusInfinity, // 8191, positive bound, no position should eval to it
};

// position evaluation score, fits in 14 bits
class Score {
    using _t = score_enum;
    using Arg = Score;
    _t v_;
public:
    static constexpr int Mask = singleton(ScoreBitSize) - 1;

    constexpr Score () : v_{NoScore} {}
    constexpr explicit Score (_t e) : v_{e} {}
    friend constexpr Score operator""_cp(unsigned long long);

    static constexpr Score clampEval(int e)  { return Score{static_cast<_t>( std::clamp<int>(e, MinEval, MaxEval) )}; }
    static constexpr Score mateLoss(Ply ply) { return Score{static_cast<_t>(MateLoss + ply.v())}; } // MateLoss + ply
    static constexpr Score mateWin(Ply ply)  { return Score{static_cast<_t>(MateWin - ply.v())}; } // MateWin - ply

    constexpr bool none() const { assert (v_ == NoScore || any()); return v_ == NoScore; }
    constexpr bool any() const { return MateLoss <= v_ && v_ <= MateWin; } // MateLoss <= v_ <= MateWin
    constexpr bool isEval() const { assert (any()); return MinEval <= v_ && v_ <= MaxEval; } // MinEval <= v_ <= MaxEval
    constexpr bool isOk(Ply ply) const { assert (any()); return mateLoss(ply) <= *this && *this < mateWin(ply); }

    // 1_ply || 1_cp
    constexpr Score minus1() const {
        assert (MateLoss <= v_); assert (v_ <= MateWin);
        //assert (v_ != MinEval); // boundary between mate and eval
        assert (v_ != MateLoss); // mate boundary
        return Score{static_cast<_t>(v_ - 1)};
    }

    constexpr Score operator - () const {
        assert (MateLoss <= v_); assert (v_ <= MateWin);
        return Score{static_cast<_t>(-v_)};
    }

    friend constexpr Score operator + (Arg a, Arg b) {
        assert (MinEval <= a.v_); assert (a.v_ <= MaxEval);
        assert (MinEval <= b.v_); assert (b.v_ <= MaxEval);
        return clampEval(a.v_ + b.v_);
    }

    friend constexpr Score operator - (Arg a, Arg b) {
        assert (MinEval <= a.v_); assert (a.v_ <= MaxEval);
        assert (MinEval <= b.v_); assert (b.v_ <= MaxEval);
        return clampEval(a.v_ - b.v_);
    }

    friend constexpr bool operator == (Arg a, Arg b) {
        assert (MateLoss <= a.v_); assert (a.v_ <= MateWin);
        assert (MateLoss <= b.v_); assert (b.v_ <= MateWin);
        return a.v_ == b.v_;
    }

    friend constexpr bool operator < (Arg a, Arg b) {
        assert (MateLoss <= a.v_); assert (a.v_ <= MateWin);
        assert (MateLoss <= b.v_); assert (b.v_ <= MateWin);
        return a.v_ < b.v_;
    }

    constexpr unsigned tt(Ply ply) const {
        auto v = v_;
        assert (v != NoScore); assert (MateLoss <= v); assert (v < MateWin);

        if (v < MinEval) {
            v = static_cast<_t>(v - ply.v());
            assert (MateLoss <= v); assert (v < MateWin);
        }
        else if (MaxEval < v) {
            v = static_cast<_t>(v + ply.v());
            assert (MateLoss <= v); assert (v < MateWin);
        }

        // convert signed to unsigned
        return static_cast<unsigned>(v - NoScore);
    }

    static constexpr Score fromTt(unsigned n, Ply ply) {
        // convert unsigned to signed
        auto v = static_cast<_t>(static_cast<int>(n) + NoScore);

        if (v == NoScore) { return Score{NoScore}; } // empty TT entry

        assert (MateLoss <= v && v < MateWin);
        if (v < MinEval) {
            v = static_cast<_t>(v + ply.v());
            if (!(v < MinEval)) {
                // probably cash collision
                assert (v < MinEval);
                return Score{NoScore};
            }
        }
        else if (MaxEval < v) {
            v = static_cast<_t>(v - ply.v());
            if (!(MaxEval < v)) {
                // probably cash collision
                assert (MaxEval < v);
                return Score{NoScore};
            }
        }

        return Score{v};
    }

    friend ostream& operator << (ostream& os, Score score) {
        os << " score ";

        if (score.none()) {
            return os << "none";
        }

        auto v = score.v_;

        if (v < MinEval) {
            assert ((v & 1) == 0); // even
            return os << "mate " << (MateLoss - v) / 2;
        }

        if (MaxEval < v) {
            assert ((v & 1) == 1); // odd
            return os << "mate " << (MateWin - v + 1) / 2;
        }

        return os << "cp " << static_cast<signed>(v);
    }
};

constexpr Score operator""_cp(unsigned long long n) { return Score{static_cast<Score::_t>(n)}; }

class PieceCountTable {
    union element_type {
        struct {
            u16_t centipawns; // material evaluation (pawn = 80)
            u8_t officers; // sum of Q = 12, R = 6, B/N = 4 (startpos total PieceMatMax = 40)
            NonKingType::arrayOf<u8_t> count; // number of pieces of the given type
        } s;
        u64_t n;
        static_assert (sizeof(s) == sizeof(n));
    };

    PieceType::arrayOf<element_type> v_;

public:
    using _t = element_type;

    consteval PieceCountTable () {
        constexpr PieceType::arrayOf<u16_t> centipawns = { 960, 480, 320, 320, 80, 0 }; // material eval: 12/6/4/4/1 * 80cp
        constexpr PieceType::arrayOf<u8_t> officers = { 12, 6, 4, 4, 0, 0 }; // non pawn pieces values

        for (auto ty : range<PieceType>()) {
            v_[ty].s.centipawns = centipawns[ty];
            v_[ty].s.officers = officers[ty];

            for (auto i : range<NonKingType>()) {
                v_[ty].s.count[i] = (ty == PieceType{i.v()});
            }
        }
    }

    constexpr _t operator[] (PieceType ty) const { return v_[ty]; }
};

extern constinit PieceCountTable pieceCountTable;

class Material {
    using _t = PieceCountTable::_t;
    _t v_;

public:
    constexpr Material () { v_.n = 0; }

    void drop(PieceType ty) { v_.n += ::pieceCountTable[ty].n; }
    void clear(NonKingType ty) { v_.n -= ::pieceCountTable[ty].n; }

    void promote(PromoType ty) {
        clear(NonKingType{Pawn});
        drop(ty);
    }

    constexpr int count(NonKingType::_t ty) const {
        return v_.s.count[NonKingType{ty}];
    }

    // any queen, rook or pawn
    constexpr bool hasMatingPieces() const {
        return count(Pawn) || count(Rook) || count(Queen);
    }

    constexpr bool canNullMove() const {
        return v_.s.officers > 0;
    }

    // [0..6] startpos = 6, queens exchanged = 4, R vs R endgame = 1
    static constexpr int gamePhase(Material my, Material op) {
        auto phase = (my.v_.s.officers + op.v_.s.officers) / 12;
        return std::min(phase, 6);
    }
};

#endif
