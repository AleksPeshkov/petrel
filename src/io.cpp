#include "io.hpp"

namespace io {

istream& fail(istream& in) {
    in.setstate(std::ios::failbit);
    return in;
}

istream& fail_char(istream& in) {
    in.unget();
    return fail(in);
}

istream& fail_pos(istream& in, std::streampos here) {
    in.clear();
    in.seekg(here);
    return fail(in);
}

istream& fail_rewind(istream& in) {
    return fail_pos(in, 0);
}

/// @brief Matches a case-insensitive token pattern in the input stream, ignoring whitespace.
/// @param in Input stream to read from.
/// @param token Token pattern to match (case-insensitive). May contain multi-word tokens
///              (e.g., "setoption name UCI_Chess960 value true").
/// @retval true Fully matches the token pattern, advancing the stream past the matched tokens.
/// @retval false Fails to match. Leaves the stream unchanged. Does not change the failbit.
bool consume(istream& in, czstring token) {
    if (token == nullptr) { token = ""; }

    auto state = in.rdstate();
    auto before = in.tellg();

    using std::isspace;
    do {
        // skip leading whitespace
        while (isspace(*token)) { ++token; }
        in >> std::ws;

        // case insensitive match each character until end of token string (whitespace or \0)
        while (!isspace(*token)
            && std::tolower(*token) == std::tolower(in.peek())
        ) {
            ++token;
            in.ignore();
        }
    // continue matching the next word in the token (if present)
    } while (isspace(*token) && isspace(in.peek()));

    // ensure the token and the last stream word ended at the same time
    if (*token == '\0'
        && (isspace(in.peek()) || in.eof())
    ) {
        // success: stream is advanced past the matched token
        return true;
    }

    // failure: restore stream state
    in.seekg(before);
    in.clear(state);
    return false;
}

bool hasMore(istream& in) {
    in >> std::ws;
    return !in.eof();
}

ostream& app_version(ostream& out) {
    out << "petrel 2.3";

#ifdef VERSION
        out << ' ' << VERSION;
#endif

#ifdef GIT_DATE
    out << ' ' << GIT_DATE;
#else
    char year[] {__DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], '\0'};

    char month[] {
        (__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't') ? '1' :
        (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') ? '1' :
        (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c') ? '1' : '0',

        (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n') ? '1' :
        (__DATE__[0] == 'F' && __DATE__[1] == 'e' && __DATE__[2] == 'b') ? '2' :
        (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r') ? '3' :
        (__DATE__[0] == 'A' && __DATE__[1] == 'p' && __DATE__[2] == 'r') ? '4' :
        (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y') ? '5' :
        (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n') ? '6' :
        (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l') ? '7' :
        (__DATE__[0] == 'A' && __DATE__[1] == 'u' && __DATE__[2] == 'g') ? '8' :
        (__DATE__[0] == 'S' && __DATE__[1] == 'e' && __DATE__[2] == 'p') ? '9' :
        (__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't') ? '0' :
        (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') ? '1' :
        (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c') ? '2' : '0',

        '\0'
    };

    char day[] {((__DATE__[4] == ' ') ? '0' : __DATE__[4]), __DATE__[5], '\0'};

    out << ' ' << year << '-' << month << '-' << day;
#endif

#ifdef GIT_ORIGIN
        out << ' ' << GIT_ORIGIN;
#endif

#ifdef GIT_SHA
        out << ' ' << GIT_SHA;
#endif

#ifdef DEBUG
        out << " DEBUG";
#endif

        return out;
    }

} // namespace io
