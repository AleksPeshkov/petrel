#include "perft.hpp"

#include "bitops128.hpp"
#include "Uci.hpp"

// unpractical overengineered transposition table replacement scheme only for experiments

class HashAge {
public:
    using _t = int;
    enum {AgeBits = 3, AgeMask = (1u << AgeBits)-1};

private:
    _t v;

public:
    constexpr HashAge () : v(1) {}
    constexpr operator const _t& () { return v; }

    void nextAge() {
        //there are "AgeMask" ages, not "1 << AgeBits", because of:
        //1) we want to break 4*n ply transposition pattern
        //2) make sure that initally clear entry is never hidden
        auto a = (v + 1) & AgeMask;
        v = a ? a : 1;
    }

};

class TtPerft : public Tt {
    using Base = Tt;

public:
    HashAge hashAge;
    const HashAge& getAge() const { return hashAge; }
    void nextAge() { hashAge.nextAge(); }

    void newGame() { Base::newGame(); hashAge = {}; }
    void newIteration() { Base::newIteration(); hashAge.nextAge(); }

    node_count_t get(Z, Ply);
    void set(Z, Ply, node_count_t);
};

class CACHE_ALIGN HashBucket {
public:
    using _t = vu64x2_t;

private:
    std::array<_t, 4> v;

public:
    constexpr HashBucket() : v{{{0,0}, {0,0}, {0,0}, {0,0}}} {}
    constexpr const _t& operator[] (int i) const { return v[i]; }

    constexpr HashBucket& operator = (const HashBucket& a) {
        v[0] = a[0];
        v[1] = a[1];
        v[2] = a[2];
        v[3] = a[3];
        return *this;
    }

    constexpr void set(int i, _t m) {
        v[i] = m;
    }

};

class PerftRecordSmall {
    u32_t key;
    u32_t perft;

public:
    constexpr void set(Z::_t z, Ply d, node_count_t n) {
        assert (small_cast<decltype(perft)>(n) == n);
        perft = static_cast<decltype(perft)>(n);

        key = makeKey(z, d);

        assert (getNodes() == n);
        assert (getDepth() == d);
    }

    constexpr static u32_t makeKey(Z::_t z, Ply d) {
        assert (d == (d & 0xf));
        return ((static_cast<decltype(key)>(z >> 32) | 0xf) ^ 0xf) | (d & 0xf);
    }

    constexpr bool isKeyMatch(Z::_t z, Ply d) const {
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
    Z::_t key;
    node_count_t nodes;

    enum { DepthBits = 6, DepthShift = 64 - DepthBits, AgeShift = DepthShift - HashAge::AgeBits };

    static const node_count_t DepthMask = static_cast<node_count_t>((1 << DepthBits)-1) << DepthShift;
    static const node_count_t AgeMask = static_cast<node_count_t>((1 << HashAge::AgeBits)-1) << AgeShift;
    static const node_count_t NodesMask = DepthMask | AgeMask;

    static constexpr node_count_t createNodes(node_count_t n, Ply d, HashAge::_t age) {
        //assert (n == (n & ~NodesMask));
        return (n & ~NodesMask) | (static_cast<decltype(nodes)>(age) << AgeShift) | (static_cast<decltype(nodes)>(d) << DepthShift);
    }

public:
    constexpr bool isKeyMatch(Z::_t z, Ply d) const {
        return (getKey() == z) && (getDepth() == d);
    }

    constexpr bool isAgeMatch(HashAge::_t age) const {
        return ((nodes & AgeMask) >> AgeShift) == static_cast<decltype(nodes)>(age);
    }

    constexpr const Z::_t& getKey() const {
        return key;
    }

    constexpr Ply getDepth() const {
        return Ply{static_cast<Ply::_t>((nodes & DepthMask) >> DepthShift)};
    }

    constexpr node_count_t getNodes() const {
        return nodes & ~NodesMask;
    }

    constexpr void set(Z::_t z, Ply d, node_count_t n, HashAge::_t age) {
        key = z;
        nodes = createNodes(n, d, age);
    }

    constexpr void setAge(HashAge::_t age) {
        nodes = (nodes & ~AgeMask) | (static_cast<decltype(nodes)>(age) << AgeShift);
    }

};

union BucketUnion {
    struct {
        std::array<PerftRecordSmall, 4> d;
        std::array<PerftRecord, 2> b;
    } v;
    HashBucket m;
};

node_count_t TtPerft::get(Z z, Ply d) {
    ++reads;

    auto origin = addr<BucketUnion>(z);
    auto u = *origin;

    if (u.v.d[0].isKeyMatch(z, d)) {
        ++hits;
        return u.v.d[0].getNodes();
    }

    if (u.v.d[1].isKeyMatch(z, d)) {
        ++hits;
        return u.v.d[1].getNodes();
    }

    if (u.v.d[2].isKeyMatch(z, d)) {
        ++hits;
        return u.v.d[2].getNodes();
    }

    if (u.v.d[3].isKeyMatch(z, d)) {
        ++hits;
        return u.v.d[3].getNodes();
    }

    if (u.v.b[0].isKeyMatch(z, d)) {
        ++hits;
        auto n = u.v.b[0].getNodes();
        if (d >= u.v.b[1].getDepth()) {
            u.v.b[0].setAge(hashAge);
            origin->m.set(3, u.m[2]);
            origin->m.set(2, u.m[3]);
        }
        return n;
    }

    if (u.v.b[1].isKeyMatch(z, d)) {
        ++hits;
        auto n = u.v.b[1].getNodes();
        return n;
    }

    return NodeCountNone;
}

void TtPerft::set(Z z, Ply d, node_count_t n) {
    ++writes;

    auto origin = addr<BucketUnion>(z);
    auto u = *origin;

    auto b0d = u.v.b[0].getDepth();

    if (u.v.b[0].isAgeMatch(hashAge) && d < b0d && n <= std::numeric_limits<u32_t>::max() && d <= 0xf) {
        //deep slots are occupied, update only short slot if possible

        if (d == 0) {
            u.v.d[0].set(z, d, n);
            origin->m.set(0, u.m[0]);
            return;
        }

        if (d == 1) {
            u.v.d[0] = u.v.d[1];
            u.v.d[1].set(z, d, n);
            origin->m.set(0, u.m[0]);
            return;
        }

        u.v.d[0] = u.v.d[1];
        u.v.d[1] = u.v.d[2];
        origin->m.set(0, u.m[0]);

        if (d >= u.v.d[3].getDepth()) {
            u.v.d[2] = u.v.d[3];
            u.v.d[3].set(z, d, n);
            origin->m.set(1, u.m[1]);
            return;
        }

        u.v.d[2].set(z, d, n);
        origin->m.set(1, u.m[1]);
        return;
    }

    if (b0d <= 4 && u.v.b[0].getNodes() <= std::numeric_limits<u32_t>::max()) {
        //the shallowest deep slot would be overwritten anyway
        //so we move it into the short slot if possible

        u.v.d[0] = u.v.d[1];
        u.v.d[1] = u.v.d[2];
        u.v.d[2] = u.v.d[3];
        u.v.d[3].set(u.v.b[0].getKey(), b0d, u.v.b[0].getNodes());
        origin->m.set(0, u.m[0]);
        origin->m.set(1, u.m[1]);
    }

    u.v.b[0].set(z, d, n, hashAge);

    if (u.v.b[1].isAgeMatch(hashAge) && d < u.v.b[1].getDepth()) {
        //move current data in the middle slot
        origin->m.set(2, u.m[2]);
        return;
    }

    //move current data in the deepest slot
    origin->m.set(2, u.m[3]);
    origin->m.set(3, u.m[2]);
}

NodePerft::NodePerft (const PositionMoves& p, Uci& r, Ply d) :
    PositionMoves{p}, parent{nullptr}, root{r}, depth{d} {}

ReturnStatus NodePerft::visitRoot() {
    NodePerft node{this};
    const auto child = &node;

    int moveCount = 0;
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : bbMovesOf(pi)) {
            auto previousPerft = perft;

            RETURN_IF_STOP (child->visitMove(from, to));

            root.info_perft_currmove(++moveCount, uciMove(from, to), perft - previousPerft);
        }
    }

    root.info_perft_depth(depth, perft);
    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::visit() {
    NodePerft node{this};
    const auto child = &node;

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : bbMovesOf(pi)) {
            RETURN_IF_STOP (child->visitMove(from, to));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::visitMove(Square from, Square to) {
    switch (depth) {
        case 0:
            perft = 1;
            break;

        case 1:
            RETURN_IF_STOP (root.limits.countNode());
            makeMoveNoZobrist(parent, from, to);
            perft = moves().popcount();
            break;

        default: {
            assert (depth >= 2);
            makeZobrist(parent, from, to);
            root.tt.prefetch(zobrist(), 64);

            RETURN_IF_STOP (root.limits.countNode());
            makeMoveNoZobrist(parent, from, to);

            perft = static_cast<TtPerft&>(root.tt).get(zobrist(), Ply{depth-2});

            if (perft == NodeCountNone) {
                perft = 0;
                RETURN_IF_STOP(visit());
                static_cast<TtPerft&>(root.tt).set(zobrist(), Ply{depth-2}, perft);
            }
        }
    }

    parent->perft += perft;
    return ReturnStatus::Continue;
}
