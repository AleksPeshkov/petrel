#include <vector>
#include <cstdint>
#include <iostream>
#include <random>
#include <iomanip>
#include <cassert>

#if defined _WIN32
#   define BB(number) number##ull
#else
#   define BB(number) number##ul
#endif

typedef unsigned index_t;
typedef std::uint32_t bit32_t;
typedef std::uint64_t bit64_t;

template <typename M, typename N>
inline M small_cast(N n) { return static_cast<M>(M(~0) & n); }

inline bit32_t low(bit64_t b)  { return small_cast<bit32_t>(b); }
inline bit32_t high(bit64_t b) { return static_cast<bit32_t>(b >> 32); }

using namespace std;

constexpr bit64_t r(bit64_t n, index_t sq) { return n << sq | n >> (64-sq); }
constexpr bit64_t o(bit64_t n) { return ~n; } //__builtin_bswap64(n)

bit64_t flip(bit64_t b) {
    return ~(b);
}

#ifdef _INCLUDE
    vector<bit64_t> deBruijn;
#else
#include "DeBruijn.inc"
#endif

typedef bit64_t U64;

U64 zobrist[64];
U64 pow2[64];

bool test_d(int d, bit64_t a, bit64_t b) {
    for (int i = 0; i < 64; ++i) {
        auto a1 = r(a, i);
        for (int j = i+1; j < 64; ++j) {
            auto b1 = r(b, j);

            for (int x = 0; x < 64; ++x) {
                if (!(r( (a1)^ (b1),x) >> (64-d))) { return false; }
                if (!(r(o(a1)^ (b1),x) >> (64-d))) { return false; }
                if (!(r( (a1)^o(b1),x) >> (64-d))) { return false; }
                if (!(r(o(a1)^o(b1),x) >> (64-d))) { return false; }
            }
        }
    }

    return true;
}

bool test_d(int d, bit64_t a, bit64_t b, bit64_t c) {
    for (int i = 0; i < 64; ++i) {
        auto a1 = r(a, i);
        for (int j = i+1; j < 64; ++j) {
            auto b1 = r(b, j);
            for (int k = j+1; k < 64; ++k) {
                auto c1 = r(c, k);

                for (int x = 0; x < 64; ++x) {
                    if (!(r( (a1)^ (b1)^ (c1),x) >> (64-d))) { return false; }
                    if (!(r(o(a1)^ (b1)^ (c1),x) >> (64-d))) { return false; }
                    if (!(r( (a1)^o(b1)^ (c1),x) >> (64-d))) { return false; }
                    if (!(r(o(a1)^o(b1)^ (c1),x) >> (64-d))) { return false; }
                    if (!(r( (a1)^ (b1)^o(c1),x) >> (64-d))) { return false; }
                    if (!(r(o(a1)^ (b1)^o(c1),x) >> (64-d))) { return false; }
                    if (!(r( (a1)^o(b1)^o(c1),x) >> (64-d))) { return false; }
                    if (!(r(o(a1)^o(b1)^o(c1),x) >> (64-d))) { return false; }
                }
            }
        }
    }

    return true;
}

bool test_d(int z, bit64_t a, bit64_t b, bit64_t c, bit64_t d) {
    for (int i = 0; i < 64; ++i) {
        auto a1 = r(a, i);
        for (int j = i+1; j < 64; ++j) {
            auto b1 = r(b, j);
            for (int k = j+1; k < 64; ++k) {
                auto c1 = r(c, k);
                for (int m = k+1; m < 64; ++m) {
                    auto d1 = r(d, m);

                    for (int x = 0; x < 64; ++x) {
                        if (!(r( (a1)^ (b1)^ (c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^ (b1)^ (c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r( (a1)^o(b1)^ (c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^o(b1)^ (c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r( (a1)^ (b1)^o(c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^ (b1)^o(c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r( (a1)^o(b1)^o(c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^o(b1)^o(c1)^ (d1),x) >> (64-z))) { return false; }
                        if (!(r( (a1)^ (b1)^ (c1)^o(d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^ (b1)^ (c1)^o(d1),x) >> (64-z))) { return false; }
                        if (!(r( (a1)^o(b1)^ (c1)^o(d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^o(b1)^ (c1)^o(d1),x) >> (64-z))) { return false; }
                        if (!(r( (a1)^ (b1)^o(c1)^o(d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^ (b1)^o(c1)^o(d1),x) >> (64-z))) { return false; }
                        if (!(r( (a1)^o(b1)^o(c1)^o(d1),x) >> (64-z))) { return false; }
                        if (!(r(o(a1)^o(b1)^o(c1)^o(d1),x) >> (64-z))) { return false; }
                    }
                }
            }
        }
    }

    return true;
}

bool test(int i1, int i2) {
    bit64_t a = deBruijn[i1];
    bit64_t b = deBruijn[i2];
    return test_d(14, a, b);
}

bool test(int i1, int i2, int i3) {
    bit64_t a = deBruijn[i1];
    bit64_t b = deBruijn[i2];
    bit64_t c = deBruijn[i3];
    return test_d(20, a, b, c);
}

bool test(int i1, int i2, int i3, int i4) {
    bit64_t a = deBruijn[i1];
    bit64_t b = deBruijn[i2];
    bit64_t c = deBruijn[i3];
    bit64_t d = deBruijn[i4];
    return test_d(28, a, b, c, d);
}

void show(int i) {
    cout << right << setfill(' ') << setw(8) << dec << i << " 0x" << setfill('0') << hex << setw(16) << deBruijn[i] << "ull,\n";
}

void findCombi() {
    int size = deBruijn.size();
    for (int a = 0; a < size; ++a) {
        cout << dec << a << endl;
        for (int b = a+1; b < size; ++b) {
            if (!test(a, b)) { continue; }

            for (int c = b+1; c < size; ++c) {
                if (!test(a, c) || !test(b, c)
                    || !test(a, a, c) || !test(a, b, c) || !test(a, c, c)
                    || !test(b, b, c)|| !test(b, c, c)
                ) { continue; }

                for (int d = c+1; d < size; ++d) {
                    if (!test(a, d) || !test(b, d) || !test(c, d)
                        || !test(a, a, d) || !test(a, b, d) || !test(a, c, d) || !test(a, d, d)
                        || !test(b, b, d) || !test(b, c, d) || !test(b, d, d)
                        || !test(c, c, d) || !test(c, d, d)
                        //|| !test(a, a, a, d) || !test(a, a, b, d) || !test(a, a, c, d) || !test(a, b, b, d) || !test(a, b, c, d) || !test(a, c, c, d) || !test(a, c, d, d) || !test(a, d, d, d)
                        //|| !test(b, b, b, d) || !test(b, b, c, d) || !test(b, b, d, d) || !test(b, c, c, d) || !test(b, c, d, d) || !test(b, d, d, d)
                    ) { continue; }

                    for (int e = d+1; e < size; ++e) {
                        if (!test(a, e) || !test(b, e) || !test(c, e) || !test(d, e)
                            || !test(a, a, e) || !test(a, b, e) || !test(a, c, e) || !test(a, d, e) || !test(a, e, e)
                            || !test(b, b, e) || !test(b, c, e) || !test(b, d, e) || !test(b, e, e)
                            || !test(c, c, e) || !test(c, d, e) || !test(c, e, e)
                            || !test(d, d, e) || !test(d, e, e)
                        ) { continue; }

                        //show(a); show(b); show(c); show(d); show(e);
                        //cout << endl;

                        for (int f = e+1; f < size; ++f) {
                            if (!test(a, f) || !test(b, f) || !test(c, f) || !test(d, f) || !test(e, f)
                                || !test(a, a, f) || !test(a, b, f) || !test(a, c, f) || !test(a, d, f) || !test(a, e, f) || !test(a, f, f)
                                || !test(b, b, f) || !test(b, c, f) || !test(b, d, f) || !test(b, e, f) || !test(b, f, f)
                                || !test(c, c, f) || !test(c, d, f) || !test(c, e, f) || !test(c, f, f)
                                || !test(d, d, f) || !test(d, e, f) || !test(d, f, f)
                                || !test(e, e, f) || !test(e, f, f)
                            ) { continue; }
                            show(a); show(b); show(c); show(d); show(e); show(f);
                            cout << endl;

                            for (int g = f+1; g < size; ++g) {
                                if (!test(a, g) || !test(b, g) || !test(c, g) || !test(d, g) || !test(e, g) || !test(f, g)
                                    || !test(a, a, g) || !test(a, b, g) || !test(a, c, g) || !test(a, d, g) || !test(a, e, g) || !test(a, f, g) || !test(a, g, g)
                                    || !test(b, b, g) || !test(b, c, g) || !test(b, d, g) || !test(b, e, g) || !test(b, f, g) || !test(b, g, g)
                                    || !test(c, c, g) || !test(c, d, g) || !test(c, e, g) || !test(c, f, g) || !test(c, g, g)
                                    || !test(d, d, g) || !test(d, e, g) || !test(d, f, g) || !test(d, g, g)
                                    || !test(e, e, g) || !test(e, f, g) || !test(e, g, g)
                                    || !test(f, f, g) || !test(f, g, g)
                                ) { continue; }
                                cout << "!!!!!!!!!!!!!!!!!!!!!!\n";
                                show(a); show(b); show(c); show(d); show(e); show(f); show(g);
                                cout << endl;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

void found(bit64_t a) {
    if (!test_d(9, a, a)) { return; }
    if (!test_d(12, a, a, a)) { return; }
    if (!test_d(19, a, a, a, a)) { return; }
    //deBruijn.push_back(a);
    cout /*<< right << setfill(' ') << setw(8) << dec << deBruijn.size()*/ << " 0x" << setfill('0') << hex << setw(16) << a << "ull,\n" << std::flush;
}

void findDeBruijn(U64 seq, int depth, int vtx, int nz) {
    const int MaxDepth = 64;
    const int MaxMask = MaxDepth-1;
    static U64 m_Lock = 0;
    if ((m_Lock & pow2[vtx]) == 0) { // only if vertex is not locked
        if ( depth == 0 ) { // depth zero, De Bruijn sequence found, see remarks
            found(seq);
        } else {
            m_Lock ^= pow2[vtx]; // set bit, lock the vertex to don't appear multiple
            if ( vtx == MaxDepth/2-1 && depth > 2 && nz <= MaxDepth/2-1 && (MaxDepth - depth) - nz <= MaxDepth/2-1) {
                findDeBruijn( seq | pow2[depth-1], depth-2, 2*vtx, nz+1);
            } else {
                if (nz <= MaxDepth/2-1) {
                    findDeBruijn( seq, depth-1, (2*vtx)&MaxMask, nz+1); // even successor
                }
                if ((MaxDepth - depth) - nz <= MaxDepth/2-1) {
                    findDeBruijn( seq | pow2[depth-1], depth-1, (2*vtx+1)&MaxMask, nz); // odd successor
                }
            }
            m_Lock ^= pow2[vtx]; // reset bit, unlock
        }
    }
}

void run() {
    for (int i=0; i < 64; i++) { pow2[i] = (U64)1 << i; }
    //findDeBruijn(0, 64-6, 0, 6);
    cout << dec << deBruijn.size() << endl;
    findCombi();
}

int main(int, const char** ) {
    run();
    return 0;

    int d;

    bit64_t table[] = {
//10/32-1/8-9/32-1/9; 8/32-1/11 8/32-1/14-8/32-1/14
0x0218a392cd5d3dbfull,
0x024530decb9f8eadull,
0x02b91efc4b53a1b3ull,
0x02dc61d5ecfc9a51ull,
0x031faf09dcda2ca9ull,
0x0352138afdd1e65bull,
0x03ac4dfb48546797ull,

//9/12/20 14/20/28
//0x0219bf251c5a7abbull,
//0x0230e2a4d67e5eddull,
//0x026e7b57e3a1948bull,
//0x031cd422b765fa4full,
//0x03c4efd5c8cda50bull,
//0x03c85a312bf66ea7ull,
//0x03ca91b316bf4277ull,

//10/12/18 14/20*
          };

    std::mt19937_64 random;
    bit64_t zobrist[2][7][64];
    for (int j=0; j < 7; ++j) {
        bit64_t b = table[j];
        for (int k=0; k < 64; ++k) {
            zobrist[0][j][k] = b;
            zobrist[1][j][k] = flip(b);
            b = b << 1 | b >> 63;

            //zobrist[0][j][k] = random();
            //zobrist[1][j][k] = random();
        }
    }

    bit64_t * z = reinterpret_cast<bit64_t*>(zobrist);
    int Size = 7 * 64;

    unsigned min = 32;
    unsigned minLo = 16;
    unsigned minLo16 = 8;
    bit64_t ave = 0;
    bit64_t aveLo = 0;
    bit64_t aveLo16 = 0;
    bit64_t count = 0;

    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }

            d = __builtin_popcountll(z[i] ^ z[j]);
            if (d > 32) { d = 64 - d; }
            if (d < min) { min = d; }
            ave += d;

            d = __builtin_popcountll(low(z[i]) ^ low(z[j]));
            if (d > 16) { d = 32 - d; }
            if (d < minLo) { minLo = d; }
            aveLo += d;

            d = __builtin_popcountll((z[i] >> 48) ^ (z[j] >> 48));
            if (d > 8) { d = 16 - d; }
            if (d < minLo16) { minLo16 = d; }
            aveLo16 += d;

            count++;
        }
    }
    std::cout << std::dec << "2 = " << min << ", ";
    std::cout << std::dec << "2Lo = " << minLo << ", ";
    std::cout << std::dec << "2Lo16 = " << minLo16 << ", ";
    std::cout << std::dec << "2Ave = " << float(ave) / count << ", ";
    std::cout << std::dec << "2AveLo = " << float(aveLo) / count << ", ";
    std::cout << std::dec << "2AveLo16 = " << float(aveLo16) / count << std::endl;

    min = 32;
    minLo = 16;
    minLo16 = 8;
    ave = 0;
    aveLo = 0;
    aveLo16 = 0;
    count = 0;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                d = __builtin_popcountll(z[i] ^ z[j] ^ z[k]);
                if (d > 32) { d = 64 - d; }
                if (d < min) { min = d; }
                ave += d;

                d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]));
                if (d > 16) { d = 32 - d; }
                if (d < minLo) { minLo = d; }
                aveLo += d;

                d = __builtin_popcountll((z[i] >> 48) ^ (z[j] >> 48) ^ (z[k] >> 48));
                if (d > 8) { d = 16 - d; }
                if (d < minLo16) { minLo16 = d; }
                aveLo16 += d;

                count++;
            }
        }
    }
    std::cout << std::dec << "3 = " << min << ", ";
    std::cout << std::dec << "3Lo = " << minLo << ", ";
    std::cout << std::dec << "3Lo16 = " << minLo16 << ", ";
    std::cout << std::dec << "3Ave = " << float(ave) / count << ", ";
    std::cout << std::dec << "3AveLo = " << float(aveLo) / count << ", ";
    std::cout << std::dec << "3AveLo16 = " << float(aveLo16) / count << std::endl;

    min = 32;
    minLo = 16;
    minLo16 = 8;
    ave = 0;
    aveLo = 0;
    aveLo16 = 0;
    count = 0;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                for (int n = 0; n < k; ++n) {
                    if (n%64 == k%64 || n%64 == j%64 || n%64 == i%64) { continue; }

                    d = __builtin_popcountll(z[i] ^ z[j] ^ z[k] ^ z[n]);
                    if (d > 32) { d = 64 - d; }
                    if (d < min) { min = d; }
                    ave += d;

                    d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]) ^ low(z[n]));
                    if (d > 16) { d = 32 - d; }
                    if (d < minLo) { minLo = d; }
                    aveLo += d;

                    d = __builtin_popcountll((z[i] >> 48) ^ (z[j] >> 48) ^ (z[k] >> 48) ^ (z[k] >> 48));
                    if (d > 8) { d = 16 - d; }
                    if (d < minLo16) { minLo16 = d; }
                    aveLo16 += d;

                    count++;
                }
            }
        }
    }
    std::cout << std::dec << "4 = " << min << ", ";
    std::cout << std::dec << "4Lo = " << minLo << ", ";
    std::cout << std::dec << "4Lo16 = " << minLo16 << ", ";
    std::cout << std::dec << "4Ave = " << float(ave) / count << ", ";
    std::cout << std::dec << "4AveLo = " << float(aveLo) / count <<  ", ";
    std::cout << std::dec << "4AveLo16 = " << float(aveLo16) / count << std::endl;

    min = 32;
    minLo = 16;
    ave = 0;
    aveLo = 0;
    count = 0;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                for (int n = 0; n < k; ++n) {
                    if (n%64 == k%64 || n%64 == j%64 || n%64 == i%64) { continue; }
                    for (int m = 0; m < n; ++m) {
                        if (m%64 == n%64 || m%64 == k%64 || m%64 == j%64 || m%64 == i%64) { continue; }

                        d = __builtin_popcountll(z[i] ^ z[j] ^ z[k] ^ z[n] ^ z[m]);
                        if (d > 32) { d = 64 - d; }
                        if (d < min) { min = d; }
                        ave += d;

                        d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]) ^ low(z[n]) ^ low(z[m]));
                        if (d > 16) { d = 32 - d; }
                        if (d < minLo) { minLo = d; }
                        aveLo += d;

                        count++;
                    }
                }
            }
        }
    }
    std::cout << std::dec << "5 = " << min << ", ";
    std::cout << std::dec << "5Lo = " << minLo << ", ";
    std::cout << std::dec << "5Ave = " << float(ave) / count << ", ";
    std::cout << std::dec << "5AveLo = " << float(aveLo) / count << std::endl;


}
