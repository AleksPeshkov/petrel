#include "perft.hpp"

#include "bitops128.hpp"
#include "Uci.hpp"
#include "Position_impl.hpp"

// unpractical overengineered transposition table replacement scheme only for experiments

class CACHE_ALIGN HashBucket {
public:
    using _t = u64x2_t;

private:
    std::array<_t, 4> v_;

public:
    constexpr HashBucket() : v_{{{0,0}, {0,0}, {0,0}, {0,0}}} {}
    constexpr _t operator[] (int i) const { return v_[i]; }

    constexpr HashBucket& operator = (const HashBucket& a) {
        v_[0] = a[0];
        v_[1] = a[1];
        v_[2] = a[2];
        v_[3] = a[3];
        return *this;
    }

    constexpr void set(int i, _t m) {
        v_[i] = m;
    }

};

class PerftRecordSmall {
    u32_t key;
    u32_t perft;

public:
    static constexpr u32_t makeKey(Z z, Ply d) {
        assert (+d == (+d & 0xf));
        return ((static_cast<decltype(key)>(+z >> 32) | 0xf) ^ 0xf) | (+d & 0xf);
    }

    constexpr void set(Z z, Ply d, node_count_t n) {
        assert (small_cast<decltype(perft)>(n) == n);
        perft = static_cast<decltype(perft)>(n);

        key = makeKey(z, d);

        assert (getNodes() == n);
        assert (getDepth() == d);
    }

    constexpr bool isKeyMatch(Z z, Ply d) const {
        return key == makeKey(z, d);
    }

    constexpr node_count_t getNodes() const {
        return perft;
    }

    constexpr Ply getDepth() const {
        return Ply{static_cast<Ply::_t>(key & 0xf)};
    }

};

class PerftRecord {
    Z key;
    node_count_t nodes;

    enum { DepthBits = 6, DepthShift = 64 - DepthBits, AgeShift = DepthShift - TtAge::bit_width() };

    static const node_count_t DepthMask = static_cast<node_count_t>((1 << DepthBits)-1) << DepthShift;
    static const node_count_t AgeMask = static_cast<node_count_t>(TtAge::mask()) << AgeShift;
    static const node_count_t NodesMask = DepthMask | AgeMask;

    static constexpr node_count_t createNodes(node_count_t n, Ply d, TtAge age) {
        //assert (n == (n & ~NodesMask));
        return (n & ~NodesMask) | (static_cast<decltype(nodes)>(+age) << AgeShift) | (static_cast<decltype(nodes)>(+d) << DepthShift);
    }

public:
    constexpr bool isKeyMatch(Z z, Ply d) const {
        return (key == z) && (getDepth() == d);
    }

    constexpr bool isAgeMatch(TtAge age) const {
        return ((nodes & AgeMask) >> AgeShift) == static_cast<decltype(nodes)>(+age);
    }

    constexpr Z getKey() const {
        return key;
    }

    constexpr Ply getDepth() const {
        return Ply{static_cast<Ply::_t>((nodes & DepthMask) >> DepthShift)};
    }

    constexpr node_count_t getNodes() const {
        return nodes & ~NodesMask;
    }

    constexpr void set(Z z, Ply d, node_count_t n, TtAge age) {
        key = z;
        nodes = createNodes(n, d, age);
    }

    constexpr void setAge(TtAge age) {
        nodes = (nodes & ~AgeMask) | (static_cast<decltype(nodes)>(+age) << AgeShift);
    }

};

union BucketUnion {
    struct {
        std::array<PerftRecordSmall, 4> d;
        std::array<PerftRecord, 2> b;
    } u;
    HashBucket m;
};

namespace {

node_count_t getTt(Z z, Ply d) {
    ++the_ttMeta.reads;

    auto* origin = the_tt.addr<BucketUnion>(z);
    auto o = *origin;

    if (o.u.d[0].isKeyMatch(z, d)) {
        ++the_ttMeta.hits;
        return o.u.d[0].getNodes();
    }

    if (o.u.d[1].isKeyMatch(z, d)) {
        ++the_ttMeta.hits;
        return o.u.d[1].getNodes();
    }

    if (o.u.d[2].isKeyMatch(z, d)) {
        ++the_ttMeta.hits;
        return o.u.d[2].getNodes();
    }

    if (o.u.d[3].isKeyMatch(z, d)) {
        ++the_ttMeta.hits;
        return o.u.d[3].getNodes();
    }

    if (o.u.b[0].isKeyMatch(z, d)) {
        ++the_ttMeta.hits;
        auto n = o.u.b[0].getNodes();
        if (d >= o.u.b[1].getDepth()) {
            o.u.b[0].setAge(the_ttMeta.age());
            origin->m.set(3, o.m[2]);
            origin->m.set(2, o.m[3]);
        }
        return n;
    }

    if (o.u.b[1].isKeyMatch(z, d)) {
        ++the_ttMeta.hits;
        auto n = o.u.b[1].getNodes();
        return n;
    }

    return NodeCountNone;
}

void setTt(Z z, Ply d, node_count_t n) {
    ++the_ttMeta.writes;

    auto origin = the_tt.addr<BucketUnion>(z);
    auto u = *origin;

    auto b0d = u.u.b[0].getDepth();

    if (u.u.b[0].isAgeMatch(the_ttMeta.age()) && d < b0d && n <= std::numeric_limits<u32_t>::max() && +d <= 0xf) {
        //deep slots are occupied, update only short slot if possible

        if (d == 0_ply) {
            u.u.d[0].set(z, d, n);
            origin->m.set(0, u.m[0]);
            return;
        }

        if (d == 1_ply) {
            u.u.d[0] = u.u.d[1];
            u.u.d[1].set(z, d, n);
            origin->m.set(0, u.m[0]);
            return;
        }

        u.u.d[0] = u.u.d[1];
        u.u.d[1] = u.u.d[2];
        origin->m.set(0, u.m[0]);

        if (d >= u.u.d[3].getDepth()) {
            u.u.d[2] = u.u.d[3];
            u.u.d[3].set(z, d, n);
            origin->m.set(1, u.m[1]);
            return;
        }

        u.u.d[2].set(z, d, n);
        origin->m.set(1, u.m[1]);
        return;
    }

    if (b0d <= 4_ply && u.u.b[0].getNodes() <= std::numeric_limits<u32_t>::max()) {
        //the shallowest deep slot would be overwritten anyway
        //so we move it into the short slot if possible

        u.u.d[0] = u.u.d[1];
        u.u.d[1] = u.u.d[2];
        u.u.d[2] = u.u.d[3];
        u.u.d[3].set(u.u.b[0].getKey(), b0d, u.u.b[0].getNodes());
        origin->m.set(0, u.m[0]);
        origin->m.set(1, u.m[1]);
    }

    u.u.b[0].set(z, d, n, the_ttMeta.age());

    if (u.u.b[1].isAgeMatch(the_ttMeta.age()) && d < u.u.b[1].getDepth()) {
        //move current data in the middle slot
        origin->m.set(2, u.m[2]);
        return;
    }

    //move current data in the deepest slot
    origin->m.set(2, u.m[3]);
    origin->m.set(3, u.m[2]);
}

} // end of anonymous namespace

ReturnStatus NodePerft::visitRoot() {
    NodePerft child{*this};

    int moveCount = 0;
    for (Pi pi : MY.any()) {
        Square from = MY.sq(pi);

        for (Square to : bbMovesOf(pi)) {
            auto previousPerft = perft;

            RETURN_IF_STOP (child.visitMove(from, to));

            the_uci.info_perft_currmove(++moveCount, toMove(from, to), perft - previousPerft);
        }
    }

    the_uci.info_perft_depth(depth, perft);
    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::visit() {
    NodePerft child{*this};

    for (Pi pi : MY.any()) {
        Square from = MY.sq(pi);

        for (Square to : bbMovesOf(pi)) {
            RETURN_IF_STOP (child.visitMove(from, to));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::visitMove(Square from, Square to) {
    switch (+depth) {
        case 0:
            perft = 1;
            break;

        case 1:
            RETURN_IF_STOP (the_uci.limits.countNode());
            makeMovePerft(parent, from, to);
            parent.clearMove(from, to);
            generateMoves();
            perft = movesTotal();
            break;

        default: {
            assert (depth >= 2_ply);
            RETURN_IF_STOP (the_uci.limits.countNode());
            makeMovePerft(parent, from, to, [&](Z z){ the_tt.prefetch<64>(z); });
            parent.clearMove(from, to);
            generateMoves();

            perft = getTt(z(), depth - 2_ply);

            if (perft == NodeCountNone) {
                perft = 0;
                RETURN_IF_STOP(visit());
                setTt(z(), depth - 2_ply, perft);
            }
        }
    }

    parent.perft += perft;
    return ReturnStatus::Continue;
}
