#include <vector>
#include <cstdint>
#include <iostream>
#include <random>
#include <iomanip>

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
U64 first;
U64 pow2[64];
int d;

bool test(int a, int a2) {
    bit64_t b = deBruijn[a];
    bit64_t x = deBruijn[a2];
    for (int i = 1; i < 64; ++i) {

        b = b << 63 | b >> 1;

        d = __builtin_popcountll(low(b) ^ low(x));
        if (!(9 <= d && d <= 23)) { return false; }

        d = __builtin_popcountll(high(b) ^ high(x));
        if (!(9 <= d && d <= 23)) { return false; }

        bit64_t o = flip(b);

        d = __builtin_popcountll(low(o) ^ low(x));
        if (!(9 <= d && d <= 23)) { return false; }

        d = __builtin_popcountll(high(o) ^ high(x));
        if (!(9 <= d && d <= 23)) { return false; }

        bit64_t b2 = x;
        for (int j = 1; j < 64; ++j) {
            b2 = b2 << 63 | b2 >> 1;

            d = __builtin_popcountll(low(b2) ^ low(b));
            if (!(8 <= d && d <= 24)) { return false; }

            d = __builtin_popcountll(high(b2) ^ high(b));
            if (!(8 <= d && d <= 24)) { return false; }

            o = flip(b2);

            d = __builtin_popcountll(low(o) ^ low(b));
            if (!(8 <= d && d <= 24)) { return false; }

            d = __builtin_popcountll(high(o) ^ high(b));
            if (!(8 <= d && d <= 24)) { return false; }
        }
    }
    return true;
}

void findCombi() {
    int d = deBruijn.size();
    for (int i1 = 0;    i1 < d; ++i1) {
        for (int i2 = i1+1; i2 < d; ++i2) {
            if (!test(i2, i1)) { continue; }
            //cout << hex << "0x" << deBruijn[i1] << "ull,\n";
            //cout << hex << "0x" << deBruijn[i2] << "ull,\n\n";
            for (int i3 = i2+1; i3 < d; ++i3) {
                if (!test(i3, i1) || !test(i3, i2)) { continue; }
                //cout << hex << "0x" << deBruijn[i1] << "ull,\n";
                //cout << hex << "0x" << deBruijn[i2] << "ull,\n";
                //cout << hex << "0x" << deBruijn[i3] << "ull,\n\n";
                for (int i4 = i3+1; i4 < d; ++i4) {
                    if (!test(i4, i1) || !test(i4, i2) || !test(i4, i3)) { continue; }
                    //cout << hex << "0x" << deBruijn[i1] << "ull,\n";
                    //cout << hex << "0x" << deBruijn[i2] << "ull,\n";
                    //cout << hex << "0x" << deBruijn[i3] << "ull,\n";
                    //cout << hex << "0x" << deBruijn[i4] << "ull,\n\n";
                    for (int i5 = i4+1; i5 < d; ++i5) {
                        if (!test(i5, i1) || !test(i5, i2) || !test(i5, i3) || !test(i5, i4)) { continue; }
                        cout << hex << "0x" << deBruijn[i1] << "ull,\n";
                        cout << hex << "0x" << deBruijn[i2] << "ull,\n";
                        cout << hex << "0x" << deBruijn[i3] << "ull,\n";
                        cout << hex << "0x" << deBruijn[i4] << "ull,\n";
                        cout << hex << "0x" << deBruijn[i5] << "ull,\n\n";
                        for (int i6 = i5+1; i6 < d; ++i6) {
                            if (!test(i6, i1) || !test(i6, i2) || !test(i6, i3) || !test(i6, i4) || !test(i6, i5)) { continue; }
                            cout << hex << "0x" << deBruijn[i1] << "ull,\n";
                            cout << hex << "0x" << deBruijn[i2] << "ull,\n";
                            cout << hex << "0x" << deBruijn[i3] << "ull,\n";
                            cout << hex << "0x" << deBruijn[i4] << "ull,\n";
                            cout << hex << "0x" << deBruijn[i5] << "ull,\n";
                            cout << hex << "0x" << deBruijn[i6] << "ull,\n\n";
                        }
                    }
                }
            }
        }
    }
}

void found(U64 seq) {
    bit64_t b = seq;
    for (int i = 1; i < 64; ++i) {
        b = b << 63 | b >> 1;

        d = __builtin_popcountll(low(b) ^ low(seq));
        if (!(11 <= d && d <= 21)) { return; }

        d = __builtin_popcountll(high(b) ^ high(seq));
        if (!(11 <= d && d <= 21)) { return; }

        bit64_t o = flip(b);

        d = __builtin_popcountll(low(o) ^ low(seq));
        if (!(9 <= d && d <= 23)) { return; }

        d = __builtin_popcountll(high(o) ^ high(seq));
        if (!(9 <= d && d <= 23)) { return; }

        bit64_t b2 = b;
        for (int j = 1; j < 64; ++j) {
            b2 = b2 << 63 | b2 >> 1;

            d = __builtin_popcountll(low(b2) ^ low(b));
            if (!(9 <= d && d <= 23)) { return; }

            d = __builtin_popcountll(high(b2) ^ high(b));
            if (!(9 <= d && d <= 23)) { return; }

            o = flip(b2);

            d = __builtin_popcountll(low(o) ^ low(b));
            if (!(9 <= d && d <= 23)) { return; }

            d = __builtin_popcountll(high(o) ^ high(b));
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
    run();
    return 0;

    int d;

    bit64_t table[] = {
//11 9 9 9 9 9 8 8
0x218a392d367abbfull,
0x218fd49de59b457ull,
0x21b2a4fd16bc773ull,
0x21b5fa77254598full,
0x23db8bf2a4d0cebull,
0x2a1ac898f3b7e97ull,
     };

    std::mt19937_64 random;
    bit64_t zobrist[2][6][64];
    for (int j=0; j < 6; ++j) {
        bit64_t b = table[j];
        for (int k=0; k < 64; ++k) {
            zobrist[0][j][k] = b;
            //zobrist[0][j][k] = random();
            b = b << 63 | b >> 1;
        }
    }
    for (int j=0; j < 6; ++j) {
        for (int k=0; k < 64; ++k) {
            //zobrist[1][j][k] = random();
            zobrist[1][j][k] = ~(zobrist[0][j][k]);
        }
    }

    bit64_t * z = reinterpret_cast<bit64_t*>(zobrist);
    const int Size = 2 * 6 * 64;

    unsigned min = 64;
    unsigned minLo = 64;
    unsigned minHi = 64;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }

            d = __builtin_popcountll(z[i] ^ z[j]);
            if (d < min) { min = d; }

            d = __builtin_popcountll(low(z[i]) ^ low(z[j]));
            if (d < minLo) { minLo = d; }

            d = __builtin_popcountll(high(z[i]) ^ high(z[j]));
            if (d < minHi) { minHi = d; }
        }
    }
    std::cout << std::dec << "2 = " << min << ", ";
    std::cout << std::dec << "2Lo = " << minLo << ", ";
    std::cout << std::dec << "2Hi = " << minHi << "\n";

    min = 64;
    minLo = 64;
    minHi = 64;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                d = __builtin_popcountll(z[i] ^ z[j] ^ z[k]);
                if (d < min) { min = d; }

                d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]));
                if (d < minLo) { minLo = d; }

                d = __builtin_popcountll(high(z[i]) ^ high(z[j]) ^ high(z[k]));
                if (d < minHi) { minHi = d; }
            }
        }
    }
    std::cout << std::dec << "3 = " << min << ", ";
    std::cout << std::dec << "3Lo = " << minLo << ", ";
    std::cout << std::dec << "3Hi = " << minHi << "\n";

    min = 64;
    minLo = 64;
    minHi = 64;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                for (int n = 0; n < k; ++n) {
                    if (n%64 == k%64 || n%64 == j%64 || n%64 == i%64) { continue; }

                    d = __builtin_popcountll(z[i] ^ z[j] ^ z[k] ^ z[n]);
                    if (d < min) { min = d; }

                    d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]) ^ low(z[n]));
                    if (d < minLo) { minLo = d; }

                    d = __builtin_popcountll(high(z[i]) ^ high(z[j]) ^ high(z[k]) ^ high(z[n]));
                    if (d < minHi) { minHi = d; }
                }
            }
        }
    }
    std::cout << std::dec << "4 = " << min << ", ";
    std::cout << std::dec << "4Lo = " << minLo << ", ";
    std::cout << std::dec << "4Hi = " << minHi << "\n";

    min = 64;
    minLo = 64;
    minHi = 64;
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
                        if (d < min) { min = d; }

                        d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]) ^ low(z[n]) ^ low(z[m]));
                        if (d < minLo) { minLo = d; }

                        d = __builtin_popcountll(high(z[i]) ^ high(z[j]) ^ high(z[k]) ^ high(z[n]) ^ low(z[m]));
                        if (d < minHi) { minHi = d; }
                    }
                }
            }
        }
    }
    std::cout << std::dec << "5 = " << min << ", ";
    std::cout << std::dec << "5Lo = " << minLo << ", ";
    std::cout << std::dec << "5Hi = " << minHi << "\n";

}
