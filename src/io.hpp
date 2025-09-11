#ifndef IO_HPP
#define IO_HPP

#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace io {
    using std::ostream;
    using std::istream;
    using std::istringstream;
    using std::ostringstream;

    using char_type = ostream::char_type;
    using czstring = const char_type* ;


    istream& fail(istream&);
    istream& fail_char(istream&);
    istream& fail_pos(istream&, std::streampos);
    istream& fail_rewind(istream&);

    bool consume(istream&, czstring);
    bool hasMore(istream&);

    ostream& app_version(ostream&);

// defined in main.cpp

    void log(const std::string&);
}

using io::ostream;
using io::istream;

#endif
