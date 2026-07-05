#include <vector>
#include <cstdint>
#include <iostream>
#include <random>
#include <iomanip>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <string>
#include <algorithm>
#include <bit>
#include <array>

#if defined _WIN32
#   define BB(number) number##ull
#else
#   define BB(number) number##ul
#endif

using u32_t = std::uint32_t;
using u64_t = std::uint64_t;

template <typename M, typename N>
inline M small_cast(N n) { return static_cast<M>(M(~0) & n); }

inline u32_t low(u64_t b)  { return small_cast<u32_t>(b); }
inline u32_t high(u64_t b) { return static_cast<u32_t>(b >> 32); }

using namespace std;

constexpr u64_t f(u64_t n) { return __builtin_bswap64(n); }
constexpr u64_t w(u64_t n, int sq) { return std::rotl(n, sq); } // bit rotate left
constexpr u64_t b(u64_t n, int sq) { return f( w(n, sq ^ 070) ); } // flip vertically
constexpr u64_t wb(u64_t n, int i) { return i < 64 ? w(n, i) : b(n, i-64); }
constexpr bool  h(u64_t n, int d) { return d <= std::popcount(n) && std::popcount(n) <= 64-d; } // humming distance threshold
constexpr bool  c(u64_t n, int sq, int d) { return (w(n, sq) >> (64-d)) == 0; } // collision

// <<<< RUNTIME DEBRUIJN LOADER/SAVER (text format) >>>>
namespace fs = std::filesystem;

vector<u64_t> deBruijn; // Global — used by found()

bool loadDeBruijn(const fs::path& filename) {
    deBruijn.clear();
    ifstream fin(filename);
    if (!fin) {
        cerr << "⚠️ File not found: " << filename << " — will generate.\n";
        return false;
    }

    string line;
    while (getline(fin, line)) {
        // Trim whitespace
        auto trim = [](string& s) {
            size_t first = s.find_first_not_of(" \t\r\n");
            if (first == string::npos) s.clear();
            else s = s.substr(first);
            size_t last = s.find_last_not_of(" \t\r\n");
            if (last != string::npos) s = s.substr(0, last + 1);
        };
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Remove optional 0x prefix
        if (line.size() >= 2 && line[0] == '0' && (line[1] == 'x' || line[1] == 'X')) {
            line = line.substr(2);
        }

        // Strip trailing "ull" (case-insensitive stripped)
        while (line.size() >= 3 &&
               (line.substr(line.size()-3) == "ull" ||
                line.substr(line.size()-3) == "ULL" ||
                line.substr(line.size()-3) == "Ull" ||
                line.substr(line.size()-3) == "uLl" ||
                line.substr(line.size()-3) == "ulL" ||
                line.substr(line.size()-3) == "Ull" )) {
            line = line.substr(0, line.size()-3);
        }

        // Convert hex string to uint64
        u64_t val = 0;
        for (char c : line) {
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
            else { val = 0; break; } // invalid char → ignore line
        }
        if (!val && !line.empty()) continue; // skip if invalid or zero (though 0x0 is valid)

        deBruijn.push_back(val);
    }

    if (deBruijn.empty()) {
        cerr << "⚠️ No valid data loaded from " << filename << "\n";
        return false;
    }

    cout << "✅ Loaded " << deBruijn.size() << " deBruijn numbers from " << filename << "\n";
    return true;
}

void saveDeBruijn(const fs::path& filename) {
    ofstream fout(filename);
    if (!fout) {
        cerr << "❌ Failed to create " << filename << "\n";
        return;
    }

    for (size_t i = 0; i < deBruijn.size(); ++i) {
        fout << hex << deBruijn[i] << "\n";
    }

    cout << "✅ Saved " << dec << deBruijn.size() << " deBruijn numbers to " << filename << "\n";
}

// collision between d adjusted bits
bool collision(int D, u64_t n1, u64_t n2) {
    for (int a = 0; a < 128; ++a)
    for (int b = a+1; b < 128; ++b)
    for (int x = 0; x < 64; ++x) {
        if (c(wb(n1, a) ^ wb(n2, b), x, D)) { return true; }
    }

    return false;
}

bool collision(int D, u64_t n1, u64_t n2, u64_t n3) {
    for (int a = 0; a < 128; ++a)
    for (int b = a+1; b < 128; ++b)
    for (int c = b+1; c < 128; ++c)
    for (int x = 0; x < 64; ++x) {
        if (::c(wb(n1, a) ^ wb(n2, b) ^ wb(n3, c), x, D)) { return true; }
    }

    return false;
}

bool hamming(u64_t n1, u64_t n2, int D) {
    for (int a = 0; a < 128; ++a)
    for (int b = a + 1; b < 128; ++b) {
        if (!h(wb(n1, a) ^ wb(n2, b), D)) { return false; }
    }
    return true;
}

bool hamming(u64_t n1, u64_t n2, u64_t n3, int D) {
    for (int a = 0; a < 128; ++a)
    for (int b = a + 1; b < 128; ++b)
    for (int c = b + 1; c < 128; ++c) {
        if (!h(wb(n1, a) ^ wb(n2, b) ^ wb(n3, c), D)) { return false; }
    }
    return true;
}

bool hamming(u64_t n1, u64_t n2, u64_t n3, u64_t n4, int D) {
    for (int a = 0; a < 128; ++a)
    for (int b = a + 1; b < 128; ++b)
    for (int c = b + 1; c < 128; ++c)
    for (int d = c + 1; d < 128; ++d) {
        if (!h(wb(n1, a) ^ wb(n2, b) ^ wb(n3, c) ^ wb(n4, d), D)) { return false; }
    }
    return true;
}

bool test(int i1, int i2) {
    u64_t a = deBruijn[i1];
    u64_t b = deBruijn[i2];
    return hamming(a, b, 18) && !collision(16, a, b);
}

bool test(int i1, int i2, int i3) {
    u64_t a = deBruijn[i1];
    u64_t b = deBruijn[i2];
    u64_t c = deBruijn[i3];
    return hamming(a, b, c, 14) && !collision(32, a, b, c);
}

bool test(int i1, int i2, int i3, int i4) {
    u64_t a = deBruijn[i1];
    u64_t b = deBruijn[i2];
    u64_t c = deBruijn[i3];
    u64_t d = deBruijn[i4];
    return hamming(a, b, c, d, 10);
}

void show(int i) {
    cout << right << setfill(' ') << setw(8) << dec << i << " 0x" << setfill('0') << hex << setw(16) << deBruijn[i] << "ull,\n";
}

void findCombi() {
    int size = deBruijn.size();
    for (int a = 0; a < size; ++a) {
        cout << dec << a << endl;
        for (int b = a+1; b < size; ++b) {
            if (!test(a, b) || !test(a, a, b) || !test(a, b, b)
                || !test(a, a, a, b) || !test(a, a, b, b) || !test(a, b, b, b)
            ) { continue; }

            for (int c = b+1; c < size; ++c) {
                if (!test(a, c) || !test(b, c)
                    || !test(a, a, c) || !test(a, b, c) || !test(b, b, c)|| !test(b, c, c) || !test(a, c, c)
                    || !test(a, a, a, c) || !test(a, a, b, c) || !test(a, b, b, c) || !test(b, b, b, c)
                    || !test(a, a, c, c) || !test(a, b, c, c) || !test(b, b, c, c) || !test(a, c, c, c) || !test(b, c, c, c)
                ) { continue; }

                for (int d = c+1; d < size; ++d) {
                    if (!test(a, d) || !test(b, d) || !test(c, d)
                        || !test(a, a, d) || !test(a, b, d) || !test(a, c, d) || !test(b, b, d) || !test(b, c, d) || !test(c, c, d)
                        || !test(a, d, d) || !test(b, d, d) || !test(c, d, d)
                        || !test(a, a, a, d) || !test(a, a, b, d) || !test(a, a, c, d) || !test(a, b, b, d) || !test(a, b, c, d) || !test(a, c, c, d)
                        || !test(b, b, b, d) || !test(b, b, c, d) || !test(b, c, c, d) || !test(c, c, c, d)
                        || !test(a, a, d, d) || !test(a, b, d, d) || !test(a, c, d, d) || !test(b, b, d, d) || !test(b, c, d, d) || !test(c, c, d, d)
                        || !test(a, d, d, d) || !test(b, d, d, d) || !test(c, d, d, d)
                    ) { continue; }

                    for (int e = d+1; e < size; ++e) {
                        if (!test(a, e) || !test(b, e) || !test(c, e) || !test(d, e)
                            || !test(a, a, e) || !test(a, b, e) || !test(a, c, e) || !test(a, d, e)
                            || !test(b, b, e) || !test(b, c, e) || !test(b, d, e)
                            || !test(c, c, e) || !test(c, d, e) || !test(d, d, e)
                            || !test(a, e, e) || !test(b, e, e) || !test(c, e, e) || !test(d, e, e)
                            || !test(a, a, a, e) || !test(a, a, b, e) || !test(a, a, c, e) || !test(a, a, d, e)
                            || !test(a, b, b, e) || !test(a, b, c, e) || !test(a, b, d, e) || !test(a, c, c, e) || !test(a, c, d, e) || !test(a, d, d, e)
                            || !test(b, b, b, e) || !test(b, b, c, e) || !test(b, b, d, e) || !test(b, c, c, e) || !test(b, c, d, e) || !test(b, d, d, e)
                            || !test(c, c, c, e) || !test(c, c, d, e) || !test(c, d, d, e) || !test(d, d, d, e)
                            || !test(a, a, e, e) || !test(a, b, e, e) || !test(a, c, e, e) || !test(a, d, e, e)
                            || !test(b, b, e, e) || !test(b, c, e, e) || !test(b, d, e, e) || !test(c, c, e, e) || !test(c, d, e, e) || !test(d, d, e, e)
                            || !test(a, e, e, e) || !test(b, e, e, e) || !test(c, e, e, e) || !test(d, e, e, e)
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
                                || !test(a, a, a, f) || !test(a, a, b, f) || !test(a, a, c, f) || !test(a, a, d, f) || !test(a, a, e, f)
                                || !test(a, b, b, f) || !test(a, b, c, f) || !test(a, b, d, f) || !test(a, b, e, f)
                                || !test(a, c, c, f) || !test(a, c, d, f) || !test(a, c, e, f)
                                || !test(a, d, d, f) || !test(a, d, e, f) || !test(a, e, e, f)
                                || !test(b, b, b, f) || !test(b, b, c, f) || !test(b, b, d, f) || !test(b, b, e, f)
                                || !test(b, c, c, f) || !test(b, c, d, f) || !test(b, c, e, f)
                                || !test(b, d, d, f) || !test(b, d, e, f) || !test(b, e, e, f)
                                || !test(c, c, c, f) || !test(c, c, d, f) || !test(c, c, e, f)
                                || !test(c, d, d, f) || !test(c, d, e, f) || !test(c, e, e, f)
                                || !test(d, d, d, f) || !test(d, d, e, f) || !test(d, e, e, f) || !test(e, e, e, f)
                                || !test(a, a, f, f) || !test(a, b, f, f) || !test(a, c, f, f) || !test(a, d, f, f) || !test(a, e, f, f) || !test(a, f, f, f)
                                || !test(b, b, f, f) || !test(b, c, f, f) || !test(b, d, f, f) || !test(b, e, f, f) || !test(b, f, f, f)
                                || !test(c, c, f, f) || !test(c, d, f, f) || !test(c, e, f, f) || !test(c, f, f, f)
                                || !test(d, d, f, f) || !test(d, e, f, f) || !test(d, f, f, f)
                                || !test(e, e, f, f) || !test(e, f, f, f)
                            ) { continue; }
                            show(a); show(b); show(c); show(d); show(e); show(f);
                            //cout << "*****\n";
                            cout << endl;
                        }
                    }
                }
            }
        }
    }
}

// temporary gloabal for test single generated de bruijn number
std::array<u64_t, 128> WB;

void init_WB(u64_t n) {
    for (int a = 0; a < 64; ++a) {
        WB[a] = w(n, a);
        WB[a+64] = b(n, a);
    }
}

constexpr bool hamming2(int D) {
    for (int a = 0; a < 128; ++a)
    for (int b = a + 1; b < 128; ++b) {
        if (!h(WB[a] ^ WB[b], D)) { return false; }
    }
    return true;
}

constexpr bool hamming3(int D) {
    for (int a = 0; a < 128; ++a)
    for (int b = a + 1; b < 128; ++b)
    for (int c = b + 1; c < 128; ++c) {
        if (!h(WB[a] ^ WB[b] ^ WB[c], D)) { return false; }
    }
    return true;
}

constexpr bool hamming4(int D) {
    for (int a = 0; a < 128; ++a)
    for (int b = a + 1; b < 128; ++b)
    for (int c = b + 1; c < 128; ++c)
    for (int d = c + 1; d < 128; ++d) {
        if (!h(WB[a] ^ WB[b] ^ WB[c] ^ WB[d], D)) { return false; }
    }
    return true;
}

void found(u64_t a) {
    init_WB(a);
    if (!hamming2(24)) { return; }
    if (!hamming3(18)) { return; }
    if (!hamming4(12)) { return; }
    if (collision(15, a, a)) { return; }
    if (collision(22, a, a, a)) { return; }
    deBruijn.push_back(a);
    cout << " 0x" << hex << setw(16) << setfill('0') << a << "\n" << std::flush;
}

void findDeBruijn(u64_t seq, int depth, int vtx, int nz) {
    // Initialize powers of 2 table
    u64_t bit[64];
    for (int i = 0; i < 64; i++) {
        bit[i] = u64_t(1) << i;
    }

    const int MaxDepth = 64;
    const int MaxMask = MaxDepth-1;
    static u64_t lock = 0;
    if ((lock & bit[vtx]) == 0) { // only if vertex is not locked
        if ( depth == 0 ) { // depth zero, De Bruijn sequence found, see remarks
            found(seq);
        } else {
            lock ^= bit[vtx]; // set bit, lock the vertex to don't appear multiple
            if ( vtx == MaxDepth/2-1 && depth > 2 && nz <= MaxDepth/2-1 && (MaxDepth - depth) - nz <= MaxDepth/2-1) {
                findDeBruijn( seq | bit[depth-1], depth-2, 2*vtx, nz+1);
            } else {
                if (nz <= MaxDepth/2-1) {
                    findDeBruijn( seq, depth-1, (2*vtx)&MaxMask, nz+1); // even successor
                }
                if ((MaxDepth - depth) - nz <= MaxDepth/2-1) {
                    findDeBruijn( seq | bit[depth-1], depth-1, (2*vtx+1)&MaxMask, nz); // odd successor
                }
            }
            lock ^= bit[vtx]; // reset bit, unlock
        }
    }
}

void run() {
    // Try to load deBruijn numbers from file
    if (!loadDeBruijn("zobrist.txt")) {
        // If load failed, generate them by running findDeBruijn
        // and then save them to file for next time
        findDeBruijn(0, 64-6, 0, 6);
        saveDeBruijn("zobrist.txt");
    }

    // Run the main benchmark logic
    findCombi();
}

int main(int, const char** ) {
    //run();
    //return 0;

    u64_t table[] = {
//10/32-1/8-9/32-1/9; 8/32-1/11 8/32-1/14-8/32-1/14
//0x0218a392cd5d3dbfull,
//0x024530decb9f8eadull,
//0x02b91efc4b53a1b3ull,
//0x02dc61d5ecfc9a51ull,
//0x031faf09dcda2ca9ull,
//0x0352138afdd1e65bull,
//0x03ac4dfb48546797ull,

// h24/12/18/24 h20/16/26/32

// 24/18/12/c15/c22 18/14/10/c16/c32
0x021ead9a5df938a3ull,
0x021ec945b9d357e3ull,
0x022a593cdfb431d7ull,
0x0270cd5bd8a47e5dull,
0x0314727ee966d5e1ull,
0x031fab2e768a4de1ull,

};


    //std::mt19937_64 random;
    u64_t zobrist[2][6][64];
    for (int j=0; j < 6; ++j) {
        u64_t n = table[j];
        for (int k=0; k < 64; ++k) {
            zobrist[0][j][k] = w(n, k);
            zobrist[1][j][k] = b(n, k);

            //zobrist[0][j][k] = random();
            //zobrist[1][j][k] = random();
        }
    }

    u64_t * z = reinterpret_cast<u64_t*>(zobrist);
    int Size = 2 * 6 * 64;

    unsigned min = 32;
    unsigned minLo = 16;
    unsigned minLo16 = 8;

    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }

            auto d = __builtin_popcountll(z[i] ^ z[j]);
            if (d > 32) { d = 64 - d; }
            if (d < min) { min = d; }

            d = __builtin_popcountll(low(z[i]) ^ low(z[j]));
            if (d > 16) { d = 32 - d; }
            if (d < minLo) { minLo = d; }

            d = __builtin_popcountll((z[i] >> 48) ^ (z[j] >> 48));
            if (d > 8) { d = 16 - d; }
            if (d < minLo16) { minLo16 = d; }
        }
    }
    std::cout << std::dec << "2 = " << min << ", ";
    std::cout << std::dec << "2Lo = " << minLo << ", ";
    std::cout << std::dec << "2Lo16 = " << minLo16 << ", ";
    std::cout << std::endl;

    min = 32;
    minLo = 16;
    minLo16 = 8;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                auto d = __builtin_popcountll(z[i] ^ z[j] ^ z[k]);
                if (d > 32) { d = 64 - d; }
                if (d < min) { min = d; }

                d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]));
                if (d > 16) { d = 32 - d; }
                if (d < minLo) { minLo = d; }

                d = __builtin_popcountll((z[i] >> 48) ^ (z[j] >> 48) ^ (z[k] >> 48));
                if (d > 8) { d = 16 - d; }
                if (d < minLo16) { minLo16 = d; }
            }
        }
    }
    std::cout << std::dec << "3 = " << min << ", ";
    std::cout << std::dec << "3Lo = " << minLo << ", ";
    std::cout << std::dec << "3Lo16 = " << minLo16 << ", ";
    std::cout << std::endl;

    min = 32;
    minLo = 16;
    minLo16 = 8;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                for (int n = 0; n < k; ++n) {
                    if (n%64 == k%64 || n%64 == j%64 || n%64 == i%64) { continue; }

                    auto d = __builtin_popcountll(z[i] ^ z[j] ^ z[k] ^ z[n]);
                    if (d > 32) { d = 64 - d; }
                    if (d < min) { min = d; }

                    d = __builtin_popcountll(low(z[i]) ^ low(z[j]) ^ low(z[k]) ^ low(z[n]));
                    if (d > 16) { d = 32 - d; }
                    if (d < minLo) { minLo = d; }

                    d = __builtin_popcountll((z[i] >> 48) ^ (z[j] >> 48) ^ (z[k] >> 48) ^ (z[k] >> 48));
                    if (d > 8) { d = 16 - d; }
                    if (d < minLo16) { minLo16 = d; }
                }
            }
        }
    }
    std::cout << std::dec << "4 = " << min << ", ";
    std::cout << std::dec << "4Lo = " << minLo << ", ";
    std::cout << std::dec << "4Lo16 = " << minLo16 << ", ";
    std::cout << std::endl;

    min = 32;
    minLo = 16;
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < i; ++j) {
            if (i%64 == j%64) { continue; }
            for (int k = 0; k < j; ++k) {
                if (k%64 == j%64 || k%64 == i%64) { continue; }
                for (int n = 0; n < k; ++n) {
                    if (n%64 == k%64 || n%64 == j%64 || n%64 == i%64) { continue; }
                    for (int m = 0; m < n; ++m) {
                        if (m%64 == n%64 || m%64 == k%64 || m%64 == j%64 || m%64 == i%64) { continue; }

                        auto d = __builtin_popcountll(z[i] ^ z[j] ^ z[k] ^ z[n] ^ z[m]);
                        if (d > 32) { d = 64 - d; }
                        if (d < min) { min = d; }

                        d = __builtin_popcountll(low(z[i] ^ z[j] ^ z[k] ^ z[n] ^ z[m]));
                        if (d > 16) { d = 32 - d; }
                        if (d < minLo) { minLo = d; }
                    }
                }
            }
        }
    }
    std::cout << std::dec << "5 = " << min << ", ";
    std::cout << std::dec << "5Lo = " << minLo << ", ";
    std::cout << std::endl;

}
