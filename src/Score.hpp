#ifndef SCORE_HPP
#define SCORE_HPP

#include "Index.hpp"

static constexpr int ScoreBitSize = 14;

// position evaluation score, fits in 14 bits
enum score_enum : i16_t {
    NoScore = -singleton(ScoreBitSize-1), //-8192 TRICK: assume two's complement
    MinusInfinity = NoScore + 1, // no position should eval to it

    MateLoss = MinusInfinity + 1, // -8190, mated in 0, only even negative values for mated positions

    // ... negative mate range of scores (loss) ...

    MinEval = MateLoss + Ply::Size, // minimal (negative) non mate score

    // ... negative position static evaluation range of scores ...

    DrawScore = 0,

    //... positive position static evalutation range of scores ...

    MaxEval = -MinEval, // maximal (positive) non mate score bound for a position

    // ... positive mate range of scores (win) ...

    MateWin = -MateLoss, // mate in 0 (impossible), only odd positive values for positions winned with mate
    MateIn1 = MateWin - 1,

    PlusInfinity = -MinusInfinity, // positive bound, no position should eval to it
};

// position evaluation score, fits in 14 bits
class Score {
    using _t = score_enum;
    _t v;
public:
    static constexpr int Mask = singleton(ScoreBitSize) - 1;

    explicit constexpr Score (_t e) : v{e} {}
    friend constexpr Score operator""_cp(unsigned long long);
    constexpr static Score clampEval(int e) { return Score{static_cast<Score::_t>( std::clamp<int>(e, MinEval, MaxEval) )}; }

    constexpr Score minus1() const { return Score{static_cast<_t>(v-1)}; }
    constexpr Score operator - () const { assertOk(); return Score{static_cast<_t>(-v)}; }
    constexpr friend Score operator + (Score s, Score d) { s.assertEval(); return Score::clampEval(s.v + d.v); }
    constexpr friend Score operator - (Score s, Score d) { s.assertEval(); return Score::clampEval(s.v - d.v); }
    constexpr friend Score operator + (Score s, Ply p) { s.assertMate(); Score r{static_cast<_t>(s.v + p)}; r.assertMate(); return r; }
    constexpr friend Score operator - (Score s, Ply p) { s.assertMate(); Score r{static_cast<_t>(s.v - p)}; r.assertMate(); return r; }

    constexpr void assertOk() const { assert (MateLoss <= v && v <= MateWin); }
    constexpr bool isEval() const { assertOk(); return MinEval <= v && v <= MaxEval; }
    constexpr void assertEval() const { assert (isEval()); }
    constexpr void assertMate() const { assert (!isEval()); }

    friend constexpr auto operator <=> (Score, Score) = default;

    // MateLoss + ply
    static constexpr Score mateLoss(Ply ply) { return Score{MateLoss} + ply; }

    // MateWin - ply
    static constexpr Score mateWin(Ply ply) { return -Score::mateLoss(ply); }

    constexpr unsigned toTt(Ply ply) const {
        Score score{v};

        if (score.v == NoScore) {
            return 0;
        }

        if (score.v < MinEval) {
            score = score - ply;
        }
        else if (MaxEval < score.v) {
            score = score + ply;
        }
        else {
            score.assertEval();
        }

        // convert signed to unsigned
        return static_cast<unsigned>(score.v - NoScore);
    }

    static constexpr Score fromTt(unsigned n, Ply ply) {
        // convert unsigned to signed
        Score score{static_cast<_t>(static_cast<int>(n) + NoScore)};

        if (score.v < MinEval) {
            score.assertMate(); score = score + ply; score.assertMate();
        }
        else if (MaxEval < score.v) {
            score.assertMate(); score = score - ply; score.assertMate();
        }
        else {
            score.assertEval();
        }

        return score;
    }

    friend ostream& operator << (ostream& out, Score score) {
        out << " score ";

        if (score == Score{NoScore}) {
            return out << "none";
        }

        score.assertOk();

        if (score.v < MinEval) {
            return out << "mate " << (MateLoss - score.v) / 2;
        }

        if (MaxEval < score.v) {
            return out << "mate " << (MateWin - score.v + 1) / 2;
        }

        score.assertEval();
        return out << "cp " << static_cast<signed>(score.v);
    }
};

constexpr Score operator""_cp(unsigned long long n) { return Score{static_cast<Score::_t>(n)}; }

//https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
class CACHE_ALIGN PieceSquareTable {
public:
    static constexpr int PieceMatMax = 32; // initial chess position sum of non pawn pieces material points

    union element_type {
        struct PACKED {
            unsigned openingPst:14;
            unsigned endgamePst:14;

            unsigned queens:4;  // number of queens
            unsigned rooks:4;   // number of rooks
            unsigned bishops:4; // number of bishops
            unsigned knights:4; // number of knights
            unsigned pawns:4;   // number of pawns

            unsigned officers:8; // sum of Q = 10, R = 5, B/N = 3 (startpos total PieceMatMax = 32)
            unsigned totalMat:8;  // sum of all pieces material points (pawn = 1)
        } s;
        u64_t v;

        constexpr auto& operator += (const element_type& o) { v += o.v; return *this; }
        constexpr auto& operator -= (const element_type& o) { v -= o.v; return *this; }

        constexpr int score(int material) const {
            auto stage = std::min(material, PieceMatMax);
            return (s.openingPst*stage + s.endgamePst*(PieceMatMax - stage));
        }
    };

protected:
    PieceType::arrayOf< Square::arrayOf<element_type> > pst;

public:
    // https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
    constexpr PieceSquareTable () {
        static_assert (sizeof(element_type) == sizeof(u64_t));

        constexpr u16_t openingMat[] = { 1025, 477, 365, 337, 82, 100 }; // PeSTO piece opening values
        constexpr u16_t endgameMat[] = { 936, 512, 297, 281, 94, 100 }; // PeSTO piece endgame values
        constexpr u8_t officers[] = { 10, 5, 3, 3, 0, 0 }; // for game phase, total sum is 32
        constexpr u8_t totalMat[] = { 12, 6, 4, 4, 1, 0 }; // for capture exchange evaluation
        constexpr u8_t queens[]   = {  1, 0, 0, 0, 0, 0 };
        constexpr u8_t rooks[]    = {  0, 1, 0, 0, 0, 0 };
        constexpr u8_t bishops[]  = {  0, 0, 1, 0, 0, 0 };
        constexpr u8_t knights[]  = {  0, 0, 0, 1, 0, 0 };
        constexpr u8_t pawns[]    = {  0, 0, 0, 0, 1, 0 };

        constexpr i16_t openingPst[PieceType::Size][Square::Size] = {
            { // Queen
                -28,   0,  29,  12,  59,  44,  43,  45,
                -24, -39,  -5,   1, -16,  57,  28,  54,
                -13, -17,   7,   8,  29,  56,  47,  57,
                -27, -27, -16, -16,  -1,  17,  -2,   1,
                -9, -26,  -9, -10,  -2,  -4,   3,  -3,
                -14,   2, -11,  -2,  -5,   2,  14,   5,
                -35,  -8,  11,   2,   8,  15,  -3,   1,
                -1, -18,  -9,  10, -15, -25, -31, -50,
            },
            { // Rook
                32,  42,  32,  51, 63,  9,  31,  43,
                27,  32,  58,  62, 80, 67,  26,  44,
                -5,  19,  26,  36, 17, 45,  61,  16,
                -24, -11,   7,  26, 24, 35,  -8, -20,
                -36, -26, -12,  -1,  9, -7,   6, -23,
                -45, -25, -16, -17,  3,  0,  -5, -33,
                -44, -16, -20,  -9, -1, 11,  -6, -71,
                -19, -13,   1,  17, 16,  7, -37, -26,
            },
            { // Bishop
                -29,   4, -82, -37, -25, -42,   7,  -8,
                -26,  16, -18, -13,  30,  59,  18, -47,
                -16,  37,  43,  40,  35,  50,  37,  -2,
                -4,   5,  19,  50,  37,  37,   7,  -2,
                -6,  13,  13,  26,  34,  12,  10,   4,
                0,  15,  15,  15,  14,  27,  18,  10,
                4,  15,  16,   0,   7,  21,  33,   1,
                -33,  -3, -14, -21, -13, -12, -39, -21,
            },
            { // Knight
                -167, -89, -34, -49,  61, -97, -15, -107,
                -73, -41,  72,  36,  23,  62,   7,  -17,
                -47,  60,  37,  65,  84, 129,  73,   44,
                -9,  17,  19,  53,  37,  69,  18,   22,
                -13,   4,  16,  13,  28,  19,  21,   -8,
                -23,  -9,  12,  10,  19,  17,  25,  -16,
                -29, -53, -12,  -3,  -1,  18, -14,  -19,
                -105, -21, -58, -33, -17, -28, -19,  -23,
            },
            { // Pawn
                0,   0,   0,   0,   0,   0,  0,   0,
                98, 134,  61,  95,  68, 126, 34, -11,
                -6,   7,  26,  31,  65,  56, 25, -20,
                -14,  13,   6,  21,  23,  12, 17, -23,
                -27,  -2,  -5,  12,  17,   6, 10, -25,
                -26,  -4,  -4, -10,   3,   3, 33, -12,
                -35,  -1, -20, -23, -15,  24, 38, -22,
                0,   0,   0,   0,   0,   0,  0,   0,
            },
            { // King
                -65,  23,  16, -15, -56, -34,   2,  13,
                29,  -1, -20,  -7,  -8,  -4, -38, -29,
                -9,  24,   2, -16, -20,   6,  22, -22,
                -17, -20, -12, -27, -30, -25, -14, -36,
                -49,  -1, -27, -39, -46, -44, -33, -51,
                -14, -14, -22, -46, -44, -30, -15, -27,
                1,   7,  -8, -64, -43, -16,   9,   8,
                -15,  36,  12, -54,   8, -28,  24,  14,
            }
        };

        constexpr i16_t endgamePst[PieceType::Size][Square::Size] = {
            { // Queen
                -9,  22,  22,  27,  27,  19,  10,  20,
                -17,  20,  32,  41,  58,  25,  30,   0,
                -20,   6,   9,  49,  47,  35,  19,   9,
                3,  22,  24,  45,  57,  40,  57,  36,
                -18,  28,  19,  47,  31,  34,  39,  23,
                -16, -27,  15,   6,   9,  17,  10,   5,
                -22, -23, -30, -16, -16, -23, -36, -32,
                -33, -28, -22, -43,  -5, -32, -20, -41,
            },
            { // Rook
                13, 10, 18, 15, 12,  12,   8,   5,
                11, 13, 13, 11, -3,   3,   8,   3,
                7,  7,  7,  5,  4,  -3,  -5,  -3,
                4,  3, 13,  1,  2,   1,  -1,   2,
                3,  5,  8,  4, -5,  -6,  -8, -11,
                -4,  0, -5, -1, -7, -12,  -8, -16,
                -6, -6,  0,  2, -9,  -9, -11,  -3,
                -9,  2,  3, -1, -5, -13,   4, -20,
            },
            { // Bishop
                -14, -21, -11,  -8, -7,  -9, -17, -24,
                -8,  -4,   7, -12, -3, -13,  -4, -14,
                2,  -8,   0,  -1, -2,   6,   0,   4,
                -3,   9,  12,   9, 14,  10,   3,   2,
                -6,   3,  13,  19,  7,  10,  -3,  -9,
                -12,  -3,   8,  10, 13,   3,  -7, -15,
                -14, -18,  -7,  -1,  4,  -9, -15, -27,
                -23,  -9, -23,  -5, -9, -16,  -5, -17,
            },
            { // Knight
                -58, -38, -13, -28, -31, -27, -63, -99,
                -25,  -8, -25,  -2,  -9, -25, -24, -52,
                -24, -20,  10,   9,  -1,  -9, -19, -41,
                -17,   3,  22,  22,  22,  11,   8, -18,
                -18,  -6,  16,  25,  16,  17,   4, -18,
                -23,  -3,  -1,  15,  10,  -3, -20, -22,
                -42, -20, -10,  -5,  -2, -20, -23, -44,
                -29, -51, -23, -15, -22, -18, -50, -64,
            },
            { // Pawn
                0,   0,   0,   0,   0,   0,   0,   0,
                178, 173, 158, 134, 147, 132, 165, 187,
                94, 100,  85,  67,  56,  53,  82,  84,
                32,  24,  13,   5,  -2,   4,  17,  17,
                13,   9,  -3,  -7,  -7,  -8,   3,  -1,
                4,   7,  -6,   1,   0,  -5,  -1,  -8,
                13,   8,   8,  10,  13,   0,   2,  -7,
                0,   0,   0,   0,   0,   0,   0,   0,
            },
            { // King
                -74, -35, -18, -18, -11,  15,   4, -17,
                -12,  17,  14,  17,  17,  38,  23,  11,
                10,  17,  23,  15,  20,  45,  44,  13,
                -8,  22,  24,  27,  26,  33,  26,   3,
                -18,  -4,  21,  24,  27,  23,   9, -11,
                -19,  -3,  11,  21,  23,  16,   7,  -9,
                -27, -11,   4,  13,  14,   4,  -5, -17,
                -53, -34, -21, -11, -28, -14, -24, -43,
            },
        };

        for (auto ty : range<PieceType>()) {
            for (auto sq : range<Square>()) {
                pst[ty][sq] = {{
                    static_cast<u16_t>(openingMat[ty] + openingPst[ty][sq]),
                    static_cast<u16_t>(endgameMat[ty] + endgamePst[ty][sq]),

                    queens[ty], rooks[ty], bishops[ty], knights[ty], pawns[ty],

                    officers[ty], totalMat[ty],
                }};
            }
        }
    }

    constexpr const element_type& operator() (PieceType ty, Square sq) const { return pst[ty][sq]; }
};

extern const PieceSquareTable pieceSquareTable;

class Evaluation {
public:
    using _t = PieceSquareTable::element_type;

private:
    _t v;

    constexpr void from(PieceType ty, Square sq) { v -= pieceSquareTable(ty, sq); }
    constexpr void to(PieceType ty, Square sq) { v += pieceSquareTable(ty, sq); }

public:
    constexpr Evaluation () : v{} {}

    // PeSTO position static evaluation
    static auto evaluate(const Evaluation& my, const Evaluation& op) {
        return (my.v.score(my.v.s.officers) - op.v.score((op.v.s.officers))) / PieceSquareTable::PieceMatMax;
    }

    void drop(PieceType ty, Square t) { to(ty, t); }
    void capture(NonKingType ty, Square f) { from(PieceType{ty}, f); }
    void move(PieceType ty, Square f, Square t) { assert (f != t); from(ty, f); to(ty, t); }

    void promote(Square f, Square t, PromoType ty) {
        assert (f.on(Rank7) && t.on(Rank8));
        from(PieceType{Pawn}, f);
        to(PieceType{ty}, t);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);

        from(PieceType{King}, kingFrom); from(PieceType{Rook}, rookFrom);
        to(PieceType{Rook}, rookTo); to(PieceType{King}, kingTo);
    }

    constexpr int count(NonKingType::_t ty) const {
        switch (ty) {
            case Queen:
                return v.s.queens;
            case Rook:
                return v.s.rooks;
            case Bishop:
                return v.s.bishops;
            case Knight:
                return v.s.knights;
            case Pawn:
                return v.s.pawns;
            default:
                assert (false);
                return 0;
        }
    }

    // any queen, rook or pawn
    constexpr bool hasMatingPieces() const {
        return (v.s.queens | v.s.rooks | v.s.pawns) != 0;
    }

    constexpr bool canNullMove() const {
        return v.s.officers > 0;
    }
};

#endif
