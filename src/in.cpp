#include "in.hpp"

namespace in {

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

} // namespace in
