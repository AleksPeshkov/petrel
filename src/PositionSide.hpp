#ifndef POSITION_SIDE_HPP
#define POSITION_SIDE_HPP

#include "PiBb.hpp"
#include "PiMask.hpp"
#include "Score.hpp"

// static information about pieces from one player's side (either side to move or its opponent)
// TRICK: all squares always relative to the point of view of given side
// (so the king piece is initially on E1 square regardless color)
class PositionSide {
    PiBb attacks_; // squares attacked by a piece and pieces attacking to a square
    PiType types; // chess type of each alive piece: king, pawn, knignt, bishop, rook, queen
    PiTrait traits; // rooks with castling rights, pawns affected by en passant, pinner pieces, checker pieces
    PiSquare squares; // onboard square locations of the alive pieces or 'NoSquare' special value

    Bb bbSide_; // bitboard of squares of all current side pieces
    Bb bbPawns_; // bitboard of squares of current side pawns
    Bb bbPawnAttacks_; // bitboard of squares attacked by pawns

    Material material_; // incremental material count
    Square opKing; // square of the opponent's king (from current side point of view)

    #ifndef NDEBUG
        constexpr void assertOk(Pi pi) const {
            types.assertOk(pi);

            Square sq = squares.sq(pi);
            assert (has(sq));

            assert (types.isPawn(pi) == isPawn(sq));
            assert (!isPawn(sq) || (!sq.on(Rank1) && !sq.on(Rank8)));

            assert (traits.isEnPassant(pi) <= types.isPawn(pi));
            assert (traits.isEnPassant(pi) <= (sq.on(Rank4) || sq.on(Rank5)));

            assert (traits.isPromotable(pi) <= types.isPawn(pi));
            assert (traits.isPromotable(pi) <= squares.sq(pi).on(Rank7));

            assert (traits.isCastling(pi) <= types.isRook(pi));
            assert (traits.isCastling(pi) <= sq.on(Rank1));
        }

        constexpr void assertOk(Pi pi, PieceType ty, Square sq) const {
            assert (squares.sq(pi) == sq);
            assert (types.typeOf(pi) == ty);
            assertOk(pi);
        }
    #else
        constexpr void assertOk(Pi) const {}
        constexpr void assertOk(Pi, PieceType, Square) const {}
    #endif

    void move(Pi, Square, Square);
    void updateMovedKing(Square);
    void setLeaperAttacks();
    void setLeaperAttack(Pi, PieceType, Square);
    void setPinner(Pi, SliderType, Square);

public:
    // incremental piece count and material score for the given side to move
    constexpr Material material() const { return material_; }

    // bitboard of squares occupied by the given side pieces
    constexpr Bb bbSide() const { assert (bbPawns_ <= bbSide_); return bbSide_; }

    // bitboard of squares occupied by the given side pawns
    constexpr Bb bbPawns() const { return bbPawns_; }

    // bitboard of squares attacked by the given side pawns
    constexpr Bb bbPawnAttacks() const { assert (bbPawnAttacks_ == bbPawns_.pForwardDiag()); return bbPawnAttacks_; }

    constexpr bool has(Square sq) const { assert (bbSide_.has(sq) == squares.has(sq)); return bbSide_.has(sq); }
    constexpr Pi pi(Square sq) const { assert (has(sq)); Pi pi = squares.pi(sq); assertOk(pi); return pi; }
    constexpr Square sq(Pi pi) const { assertOk(pi); return squares.sq(pi); }
    constexpr Square sqKing() const { return sq(Pi{TheKing}); } // sq(TheKing)
    constexpr bool isKing(Square sq) const { return sqKing().is(sq); } // sq(TheKing)
    constexpr PiMask piecesOn(Rank::_t rank) const { Rank{rank}.assertOk(); return squares.piecesOn(rank); }

    constexpr PieceType typeOf(Pi pi) const { assertOk(pi); return types.typeOf(pi); }
    constexpr PieceType typeAt(Square sq) const { return typeOf(pi(sq)); }

    // all onboard pieces of the given side
    constexpr PiMask pieces() const { assert (squares.pieces() == types.pieces()); return squares.pieces(); }
    constexpr PiMask sliders() const { return types.sliders(); } // Q, R, B
    constexpr PiMask officers() const { return types.officers(); } // Q, R, B, N
    constexpr PiMask nonPawns() const { return types.nonPawns(); } // K, Q, R, B, N
    constexpr PiMask nonKing() const { return types.nonKing(); } // Q, R, B, N, P
    constexpr PiMask pawns() const { return types.piecesOf(Pawn); }

    // pieces of less value than given piece type
    constexpr PiMask lessValue(PieceType ty) const { return types.lessValue(ty); }

    // pieces of less or equal value than given piece type
    constexpr PiMask lessOrEqualValue(PieceType ty) const { return types.lessOrEqualValue(ty); }

    constexpr PiMask castlingRooks() const { return traits.castlingRooks(); }
    constexpr bool isCastling(Pi pi) const { assertOk(pi); return traits.isCastling(pi); }
    constexpr bool isCastling(Square sq) const { return isCastling(pi(sq)); }

    constexpr bool isPawn(Square sq) const { return bbPawns_.has(sq); }
    constexpr PiMask promotables() const { return traits.promotables(); } // pawns on the 7th rank
    constexpr bool isPromotable(Pi pi) const { assertOk(pi); return traits.isPromotable(pi); } // is pawn and on the 7th rank

    constexpr PiMask enPassantPawns() const { return traits.enPassantPawns(); }
    constexpr bool isEnPassant(Pi pi) const { return traits.isEnPassant(pi); }
    constexpr bool hasEnPassant() const { return enPassantPawns().any(); }
    constexpr Square sqEnPassant() const { Square ep{sq(traits.piEnPassant())}; assert (ep.on(Rank4)); return ep; }
    constexpr File fileEnPassant() const { return sqEnPassant().file(); }

    constexpr const auto& attacks() const { return attacks_; }
    PiMask attackersTo(Square sq) const { return attacks_.piMask(sq); }
    PiMask affectedBy(Square sq) const { return attackersTo(sq); }
    PiMask affectedBy(Square a, Square b) const { return affectedBy(a) | affectedBy(b); }
    PiMask affectedBy(Square a, Square b, Square c) const { return affectedBy(a) | affectedBy(b) | affectedBy(c); }
    int countAttackersTo(Square, Bb) const; // total number of attackers (including X-ray)

    PiMask checkers() const { assert (traits.checkers() == attackersTo(opKing)); return traits.checkers(); }
    constexpr PiMask pinners() const { return traits.pinners(); }
    bool isPinned(Bb) const;

    constexpr HistoryType historyType(Square from, Square to) const {
        // any pawn move or castling or null move
        //TRICK: from == to can be either null move or rook underpromotion
        if (from == to || isPawn(from) || isKing(to)) { return HistoryType{HistorySpecial}; }

        constexpr HistoryType::_t fromPieceType[] = { HistoryQN, HistoryRB, HistoryRB, HistoryQN, HistorySpecial, HistoryKing };
        return HistoryType{fromPieceType[+typeAt(from)]};
    }

    constexpr bool isPseudoLegal(HistoryMove move) const {
        if (move.none()) { return false; }

        Square from{move.from()};
        Square to{move.to()};

        return has(from) && move.historyType() == historyType(from, to);
    }

//friend class Position;
    static void swap(PositionSide&, PositionSide&);

    void setOpKing(Square);
    void move(Pi, PieceType, Square, Square);
    void movePawn(Square, Square);
    void moveKing(Square, Square);
    void castle(Square kingFrom, Square kingTo, Pi rook, Square rookFrom, Square rookTo);
    Pi piPromoted(Square, PromoType, Square);
    void capture(Square);

    void setEnPassantVictim(Square);
    void setEnPassantKiller(Square);
    void clearEnPassantVictim();
    void clearEnPassantKillers();
    void clearCheckers() { traits.clearCheckers(); }

    void updateSliders(PiMask, Bb);
    void updateSlidersCheckers(PiMask, Bb);

    //used only during initial position setup
    bool dropValid(PieceType, Square);
    static void finalSetup(PositionSide&, PositionSide&);

// friend class UciPosition;
    bool setValidCastling(File);
    bool setValidCastling(CastlingSide);
};

#endif
