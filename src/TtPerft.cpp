#include "bitops128.hpp"
#include "TtPerft.hpp"
#include "Zobrist.hpp"

class CACHE_ALIGN HashBucket {
public:
    using _t = vu64x2_t;
    using Index = ::Index<4>;

private:
    Index::arrayOf<_t> v;

public:
    constexpr HashBucket() : v{{{0,0}, {0,0}, {0,0}, {0,0}}} {}
    constexpr const _t& operator[] (Index::_t i) const { return v[i]; }

    constexpr HashBucket& operator = (const HashBucket& a) {
        v[0] = a[0];
        v[1] = a[1];
        v[2] = a[2];
        v[3] = a[3];
        return *this;
    }

    constexpr void set(Index::_t i, _t m) {
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
        return key & 0xf;
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
        return (nodes & DepthMask) >> DepthShift;
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
        Index<4>::arrayOf<PerftRecordSmall> d;
        Index<2>::arrayOf<PerftRecord> b;
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
