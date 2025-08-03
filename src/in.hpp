#ifndef IN_HPP
#define IN_HPP

#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace in {
    using std::istream;

    typedef istream::char_type char_type;
    typedef const char_type* czstring;

    istream& fail(istream&);
    istream& fail_char(istream&);
    istream& fail_pos(istream&, std::streampos);
    istream& fail_rewind(istream&);

    bool next(istream&, czstring);
    bool nextNothing(istream&);
}

#endif
