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
constexpr bit64_t o(bit64_t n) { return ~__builtin_bswap64(n); }

bit64_t flip(bit64_t b) {
    return ~(b);
}

vector<bit64_t> deBruijn;

typedef bit64_t U64;

U64 zobrist[64];
U64 pow2[64];
/*
bool test(int i1, int i2) {
    int d;
    bit64_t a = deBruijn[i2];
    bit64_t b = deBruijn[i1];

    for (int i = 0; i < 64; ++i) {

        d = __builtin_popcountll(low(b) ^ low(a));
        if (!(8 <= d && d <= 24)) { return false; }

        d = __builtin_popcountll((b >> 53) ^ (a >> 53));
        if (!(1 <= d && d <= 10)) { return false; }

        a = a << 1 | a >> 63;
        b = b << 1 | b >> 63;
    }

    for (int i = 1; i < 64; ++i) {

        b = b << 1 | b >> 63;

        d = __builtin_popcountll(low(b) ^ low(a));
        if (!(8 <= d && d <= 24)) { return false; }

        d = __builtin_popcountll((b >> 50) ^ (a >> 50));
        if (!(1 <= d && d <= 13)) { return false; }

        bit64_t b2 = a;
        for (int j = 1; j < 64; ++j) {
            b2 = b2 << 1 | b2 >> 63;

            d = __builtin_popcountll(low(b2) ^ low(b));
            if (!(8 <= d && d <= 24)) { return false; }

            d = __builtin_popcountll((b2 >> 50) ^ (b >> 50));
            if (!(1 <= d && d <= 13)) { return false; }
        }
    }
    return true;
}
*/

bool test_d(int d, bit64_t a, bit64_t b) {
    auto c1 =    a ^ b;
    auto c2 = o(a) ^ b;
    auto c3 =    a ^ o(b);
    auto c4 = o(a) ^ o(b);

    for (int i = 0; i < 64; ++i) {
        int p1 = __builtin_popcountll(c1 >> (64-d));
        int p2 = __builtin_popcountll(c2 >> (64-d));
        int p3 = __builtin_popcountll(c3 >> (64-d));
        int p4 = __builtin_popcountll(c4 >> (64-d));
        if (!(1 <= p1 && p1 <= (d-1))) { return false; }
        if (!(1 <= p2 && p2 <= (d-1))) { return false; }
        if (!(1 <= p3 && p3 <= (d-1))) { return false; }
        if (!(1 <= p4 && p4 <= (d-1))) { return false; }
        c1 = r(c1, 1);
        c2 = r(c2, 1);
        c3 = r(c3, 1);
        c4 = r(c4, 1);
    }

    return true;
}

bool test_d(int d, bit64_t a, bit64_t b, bit64_t c) {
    auto c1 =    a ^  b   ^ c;
    auto c2 = o(a) ^  b   ^ c;
    auto c3 =    a ^ o(b) ^ c;
    auto c4 = o(a) ^ o(b) ^ c;
    auto c5 =    a ^  b   ^ o(c);
    auto c6 = o(a) ^  b   ^ o(c);
    auto c7 =    a ^ o(b) ^ o(c);
    auto c8 = o(a) ^ o(b) ^ o(c);

    for (int i = 0; i < 64; ++i) {
        int p1 = __builtin_popcountll(c1 >> (64-d));
        int p2 = __builtin_popcountll(c2 >> (64-d));
        int p3 = __builtin_popcountll(c3 >> (64-d));
        int p4 = __builtin_popcountll(c4 >> (64-d));
        int p5 = __builtin_popcountll(c5 >> (64-d));
        int p6 = __builtin_popcountll(c6 >> (64-d));
        int p7 = __builtin_popcountll(c7 >> (64-d));
        int p8 = __builtin_popcountll(c8 >> (64-d));
        if (!(1 <= p1 && p1 <= (d-1))) { return false; }
        if (!(1 <= p2 && p2 <= (d-1))) { return false; }
        if (!(1 <= p3 && p3 <= (d-1))) { return false; }
        if (!(1 <= p4 && p4 <= (d-1))) { return false; }
        if (!(1 <= p5 && p5 <= (d-1))) { return false; }
        if (!(1 <= p6 && p6 <= (d-1))) { return false; }
        if (!(1 <= p7 && p7 <= (d-1))) { return false; }
        if (!(1 <= p8 && p8 <= (d-1))) { return false; }
        c1 = r(c1, 1);
        c2 = r(c2, 1);
        c3 = r(c3, 1);
        c4 = r(c4, 1);
        c5 = r(c5, 1);
        c6 = r(c6, 1);
        c7 = r(c7, 1);
        c8 = r(c8, 1);
    }

    return true;
}

bool test(int i1, int i2) {
    bit64_t a = deBruijn[i1];
    bit64_t b = deBruijn[i2];
    for (int i = 1; i < 64; ++i) {
        a = a << 1 | a >> 63;
        b = b << 1 | b >> 63;

        if (!test_d(a, b, 12)) { return false; }

        /*bit64_t a2 = a;
        bit64_t b2 = b;
        for (int j = i+1; j < 64; ++j) {
            a2 = a2 << 1 | a2 >> 63;
            b2 = b2 << 1 | b2 >> 63;
            if (!test_d(deBruijn[i1]^b^b2, 16)) { return false; }
            if (!test_d(deBruijn[i2]^a^a2, 16)) { return false; }
        }*/
    }
    return true;
}

void show(int i) {
    cout << right << setfill(' ') << setw(8) << dec << i << " 0x" << setfill('0') << hex << setw(16) << deBruijn[i] << "ull,\n";
}

void findCombi() {
    int size = deBruijn.size();
    for (int a = 0; a < size; ++a) {
        for (int b = a+1; b < size; ++b) {
            if (!test(b, a)) { continue; }

            for (int c = b+1; c < size; ++c) {
                if (!test(c, b) || !test(c, a)) { continue; }

                for (int d = c+1; d < size; ++d) {
                    if (!test(d, c) || !test(d, b) || !test(d, a)) { continue; }

                    for (int e = d+1; e < size; ++e) {
                        if (!test(e, d) || !test(e, c) || !test(e, b) || !test(e, a)) { continue; }

                        for (int f = e+1; f < size; ++f) {
                            if (!test(f, e) || !test(f, d) || !test(f, c) || !test(f, b) || !test(f, a)) { continue; }
                            show(a); show(b); show(c); show(d); show(e); show(f);
                            cout << endl;

                            for (int g = f+1; g < size; ++g) {
                                if (!test(g, f) || !test(g, e) || !test(g, d) || !test(g, c) || !test(g, b) || !test(g, a)) { continue; }
                                cout << "!!!!!!!!!!!!!!!!!!!!!!\n";
                                show(a); show(b); show(c); show(d); show(e); show(f); show(g);
                                cout << endl;
                                return;

                                for (int h = g+1; h < size; ++h) {
                                    if (!test(h, g) || !test(h, f) || !test(h, e) || !test(h, d) || !test(h, c) || !test(h, b) || !test(h, a)) { continue; }
                                    show(a); show(b); show(c); show(d); show(e); show(f); show(g); show(h);
                                    cout << endl;

                                    for (int i = h+1; i < size; ++i) {
                                        if (!test(i, h) || !test(i, g) || !test(i, f) || !test(i, e) || !test(i, d) || !test(i, c) || !test(i, b) || !test(i, a)) { continue; }
                                        cout << "######################\n";
                                        show(a); show(b); show(c); show(d); show(e); show(f); show(g); show(h); show(i);
                                        cout << endl;

                                        for (int j = i+1; j < size; ++j) {
                                            if (!test(j, i) || !test(j, h) || !test(j, g) || !test(j, f) || !test(j, e) || !test(j, d) || !test(j, c) || !test(j, b) || !test(j, a)) { continue; }
                                            cout << "######################\n";
                                            show(a); show(b); show(c); show(d); show(e); show(f); show(g); show(h); show(i); show(j);
                                            cout << endl;
                                        }

                                    }

                                }
                            }
                        }
                    }
                }
            }
            //show(a); show(b);
            //cout << endl;
        }
    }
}

void found(bit64_t a) {
    //cout << '.';
    for (index_t i = 1; i < 64; ++i) {
        auto b = r(a, i);
        if (!test_d(12, a, b)) { return; }

        for (int j = i+1; j < 64; ++j) {
            auto c = r(a, j);
            if (!test_d(16, a, b, c)) { return; }
        }
    }
    deBruijn.push_back(a);
    //cout << right << setfill(' ') /*<< setw(8) << dec << i*/ << " 0x" << setfill('0') << hex << setw(16) << a << "ull,\n" << std::flush;
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
    findDeBruijn(0, 64-6, 0, 6);
    cout << dec << deBruijn.size() << endl;
    findCombi();
}

int main(int, const char** ) {
    run();
    return 0;

    int d;

    bit64_t table[] = {
//10/32-1/8-9/32-1/9; 8/32-1/11 8/32-1/16-8/32-1/16
//0x0218a392cd5d3dbfull,
//0x024530decb9f8eadull,
//0x02896e9abd8e19f9ull,
//0x02d0d9129eaefc73ull,
//0x034fd784b731d915ull,
//0x03b27e8a5bcc6ae1ull,
//0x03ca242d98d3bf57ull,

//10/32-1/8-9/32-1/9; 8/32-1/11 8/32-1/14-8/32-1/14
0x0218a392cd5d3dbfull,
0x024530decb9f8eadull,
0x02b91efc4b53a1b3ull,
0x02dc61d5ecfc9a51ull,
0x031faf09dcda2ca9ull,
0x0352138afdd1e65bull,
0x03ac4dfb48546797ull,
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
