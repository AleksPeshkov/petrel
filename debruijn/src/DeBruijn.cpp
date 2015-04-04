#include <vector>
#include <cstdint>
#include <iostream>
#include <random>
#include <iomanip>
#include <cassert>

typedef unsigned index_t;
typedef std::uint32_t bit32_t;
typedef std::uint64_t bit64_t;

template <typename M, typename N>
inline M small_cast(N n) { return static_cast<M>(M(~0) & n); }

inline bit32_t low(bit64_t b)  { return small_cast<bit32_t>(b); }
inline bit32_t high(bit64_t b) { return static_cast<bit32_t>(b >> 32); }

using namespace std;

bit64_t flip(bit64_t b) {
    return ~(b);
}

vector<bit64_t> deBruijn;

typedef bit64_t U64;

U64 zobrist[64];
U64 pow2[64];

bool test(int a, int a2) {
    int d;
    bit64_t b = deBruijn[a];
    bit64_t x = deBruijn[a2];
    for (int i = 1; i < 64; ++i) {

        b = b << 1 | b >> 63;

        d = __builtin_popcountll(low(b) ^ low(x));
        if (!(9 <= d && d <= 23)) { return false; }

        d = __builtin_popcountll(high(b) ^ high(x));
        if (!(9 <= d && d <= 23)) { return false; }

        bit64_t b2 = x;
        for (int j = 1; j < 64; ++j) {
            b2 = b2 << 1 | b2 >> 63;

            d = __builtin_popcountll(low(b2) ^ low(b));
            if (!(8 <= d && d <= 24)) { return false; }

            d = __builtin_popcountll(high(b2) ^ high(b));
            if (!(8 <= d && d <= 24)) { return false; }
        }
    }
    return true;
}

void show(int i) {
    cout << right << setfill(' ') << setw(6) << dec << i << " 0x" << setfill('0') << hex << setw(16) << deBruijn[i] << "ull,\n";
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

                            for (int g = f+1; g < size; ++g) {
                                if (!test(g, f) || !test(g, e) || !test(g, d) || !test(g, c) || !test(g, b) || !test(g, a)) { continue; }
                                //show(a); show(b); show(c); show(d); show(e); show(f); show(g);
                                //cout << endl;

                                for (int h = g+1; h < size; ++h) {
                                    if (!test(h, g) || !test(h, f) || !test(h, e) || !test(h, d) || !test(h, c) || !test(h, b) || !test(h, a)) { continue; }
                                    cout << "!!!!!!!!!!!!!!!!!!!!!!\n";
                                    show(a); show(b); show(c); show(d); show(e); show(f); show(g); show(h);
                                    cout << endl;
                                    return;

                                    /*for (int i = h+1; i < size; ++i) {
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

                                    }*/

                                }
                            }
                        }
                    }
                }
            }
            show(a); show(b);
            cout << endl;
        }
    }
}

void found(U64 seq) {
    int d;
    bit64_t b = seq;
    for (int i = 1; i < 64; ++i) {
        b = b << 1 | b >> 63;

        d = __builtin_popcountll(low(b) ^ low(seq));
        if (!(11 <= d && d <= 21)) { return; }

        d = __builtin_popcountll(high(b) ^ high(seq));
        if (!(11 <= d && d <= 21)) { return; }

        bit64_t b2 = b;
        for (int j = 1; j < 64; ++j) {
            b2 = b2 << 1 | b2 >> 63;

            d = __builtin_popcountll(low(b2) ^ low(b));
            if (!(9 <= d && d <= 23)) { return; }

            d = __builtin_popcountll(high(b2) ^ high(b));
            if (!(9 <= d && d <= 23)) { return; }

        }

    }
    deBruijn.push_back(seq);
    //cout << hex << "0x" << seq << "ull,\n";
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
//deBruijn.push_back(0x218a392d367abbfull);
//deBruijn.push_back(0x218fd49de59b457ull);
//deBruijn.push_back(0x21b2a4fd16bc773ull);
//deBruijn.push_back(0x21b5fa77254598full);
//deBruijn.push_back(0x23db8bf2a4d0cebull);
//deBruijn.push_back(0x2a1ac898f3b7e97ull);
    findCombi();
}

int main(int, const char** ) {
    //run();
    //return 0;

    int d;

    bit64_t table[] = {
//11 9 9 8
0x0218a392d367abbfull,
0x0218fd49de59b457ull,
0x021b2a4fd16bc773ull,
0x026763d5c37e5a45ull,
0x0323dba73562fc25ull,
0x032fc73dbac2a4d1ull,
0x03422eadec73253full,
     };

    std::mt19937_64 random;
    bit64_t zobrist[2][7][64];
    for (int j=0; j < 7; ++j) {
        bit64_t b = table[j];
        for (int k=0; k < 64; ++k) {
            zobrist[0][j][k] = b;
            zobrist[1][j][k] = flip(b);
            b = b << 1 | b >> 63;

            zobrist[0][j][k] = random();
            //zobrist[1][j][k] = random();
        }
    }

    bit64_t * z = reinterpret_cast<bit64_t*>(zobrist);
    int Size = 2 * 7 * 64;

    unsigned min = 32;
    unsigned minLo = 16;
    unsigned ave = 0;
    unsigned aveLo = 0;
    unsigned count = 0;

    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }

            d = __builtin_popcountll(z[i] ^ z[j]);
            //if (d > 32) { d = 64 - d; }
            if (d < min) { min = d; }
            ave += d;

            d = __builtin_popcountll(low(z[i]) ^ low(z[j]));
            //if (d > 16) { d = 32 - d; }
            if (d < minLo) { minLo = d; }
            aveLo += d;

            count++;
        }
    }
    std::cout << std::dec << "2 = " << min << ", ";
    std::cout << std::dec << "2Lo = " << minLo << ", ";
    std::cout << std::dec << "2Ave = " << float(ave) / count << ", ";
    std::cout << std::dec << "2AveLo = " << float(aveLo) / count << std::endl;

    min = 32;
    minLo = 16;
    ave = 0;
    aveLo = 0;
    count = 0;
    Size = Size/2;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                d = __builtin_popcountll(z[i] ^ z[j] ^ z[k]);
                //if (d > 32) { d = 64 - d; }
                if (d < min) { min = d; }
                ave += d;

                d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]));
                //if (d > 16) { d = 32 - d; }
                if (d < minLo) { minLo = d; }
                aveLo += d;

                count++;
            }
        }
    }
    std::cout << std::dec << "3 = " << min << ", ";
    std::cout << std::dec << "3Lo = " << minLo << ", ";
    std::cout << std::dec << "3Ave = " << float(ave) / count << ", ";
    std::cout << std::dec << "3AveLo = " << float(aveLo) / count << std::endl;

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

                    d = __builtin_popcountll(z[i] ^ z[j] ^ z[k] ^ z[n]);
                    //if (d > 32) { d = 64 - d; }
                    if (d < min) { min = d; }
                    ave += d;

                    d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]) ^ low(z[n]));
                    //if (d > 16) { d = 32 - d; }
                    if (d < minLo) { minLo = d; }
                    aveLo += d;

                    count++;
                }
            }
        }
    }
    std::cout << std::dec << "4 = " << min << ", ";
    std::cout << std::dec << "4Lo = " << minLo << ", ";
    std::cout << std::dec << "4Ave = " << float(ave) / count << ", ";
    std::cout << std::dec << "4AveLo = " << float(aveLo) / count << std::endl;

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
