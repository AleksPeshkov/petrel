#ifndef CASTLING_RULES_HPP
#define CASTLING_RULES_HPP

#include "Bb.hpp"

class CastlingRules {
    struct Rules {
        Bb unimpeded;
        Bb unattacked;
    };

    File::arrayOf< File::arrayOf<Rules> > castlingRules;

    static constexpr Bb exBetween(Square king, Square rook) { return ::inBetween(king, rook) + rook; }

public:
    constexpr CastlingRules () {
        FOR_EACH(File, kingFile) {
            FOR_EACH(File, rookFile) {
                Square king{kingFile, Rank1};
                Square rook{rookFile, Rank1};

                if (king == rook) {
                    castlingRules[kingFile][rookFile].unimpeded  = Bb{};
                    castlingRules[kingFile][rookFile].unattacked = Bb{};
                    continue;
                }

                switch (CastlingRules::castlingSide(king, rook)) {
                    case QueenSide:
                        castlingRules[kingFile][rookFile].unimpeded  = (exBetween(king, C1) | exBetween(rook, D1)) % (Bb{king} + rook);
                        castlingRules[kingFile][rookFile].unattacked = exBetween(king, C1) | king;
                        break;

                    case KingSide:
                        castlingRules[kingFile][rookFile].unimpeded  = (exBetween(king, G1) | exBetween(rook, F1)) % (Bb{king} + rook);
                        castlingRules[kingFile][rookFile].unattacked = exBetween(king, G1) | king;
                        break;
                }
            }
        }
    }

    bool isLegal(Square king, Square rook, Bb occupied, Bb attacked) const {
        assert (king.on(Rank1));
        assert (rook.on(Rank1));
        assert (king != rook);
        return (occupied & castlingRules[File(king)][File(rook)].unimpeded).none() && (attacked & castlingRules[File(king)][File(rook)].unattacked).none();
    }

    static constexpr CastlingSide castlingSide(Square king, Square rook) {
        return (rook < king) ? QueenSide : KingSide;
    }

    static constexpr Square castlingKingTo(Square king, Square rook) {
        return castlingSide(king, rook).is(QueenSide) ? C1 : G1;
    }

    static constexpr Square castlingRookTo(Square king, Square rook) {
        return castlingSide(king, rook).is(QueenSide) ? D1 : F1;
    }

};

extern const CastlingRules castlingRules;

#endif
