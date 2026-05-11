#ifndef IO_HPP
#define IO_HPP

#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include "common.hpp"

namespace io {
    using std::ostream;
    using std::istream;
    using std::istringstream;
    using std::ostringstream;

    using char_type = ostream::char_type;
    using czstring = const char_type* ;

    COLD istream& fail(istream&);
    COLD istream& fail_char(istream&);
    COLD istream& fail_pos(istream&, std::streampos);
    COLD istream& fail_rewind(istream&);

    bool consume(istream&, czstring);
    bool hasMore(istream&);

    ostream& app_version(ostream&);

// defined in main.cpp

    COLD void error(std::string_view);
    COLD void info(std::string_view);
}

using io::ostream;
using io::istream;

#endif
