#ifndef IO_HPP
#define IO_HPP

#include <iostream>
#include <string_view>
#include "common.hpp"

namespace io {
    using std::ostream;
    using std::istream;

    using char_type = ostream::char_type;
    using czstring = const char_type*;

// defined in Uci.cpp

    COLD istream& fail(istream&);
    COLD istream& fail_char(istream&);
    COLD istream& fail_pos(istream&, std::streampos);
    COLD istream& fail_rewind(istream&);

    bool consume(istream&, czstring);

// defined in main.cpp

    ostream& app_version(ostream&);
    COLD void error(std::string_view);
}

using io::ostream;
using io::istream;

#endif
