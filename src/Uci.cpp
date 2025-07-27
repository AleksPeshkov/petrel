#include "Uci.hpp"
#include "chrono.hpp"
#include "UciGoLimit.hpp"

namespace {
    ostream& uci_error(ostream& err, io::istream& context) {
        return err << "parsing error: " << context.rdbuf() << '\n';
    }

    io::istream& operator >> (io::istream& in, TimeInterval& timeInterval) {
        unsigned long msecs;
        if (in >> msecs) {
            timeInterval = duration_cast<TimeInterval>( Msecs{msecs} );
        }
        return in;
    }
}

void Uci::operator() (io::istream& in, ostream& err) {
    for (std::string currentLine; std::getline(in, currentLine); ) {
        command.clear(); //clear state from the previous command
        command.str(std::move(currentLine));
        command >> std::ws;

        if      (next("go"))        { go(); }
        else if (next("position"))  { position(); }
        else if (next("stop"))      { root.stop(); }
        else if (next("isready"))   { root.isready(); }
        else if (next("setoption")) { setoption(); }
        else if (next("set"))       { setoption(); }
        else if (next("ucinewgame")){ ucinewgame(); }
        else if (next("uci"))       { root.uciok(); }
        else if (next("perft"))     { goPerft(); }
        else if (next("quit"))      { break; }
        else if (next("exit"))      { break; }

        //parsing error detected or something left unparsed
        if (!nextNothing()) {
            uci_error(err, command);
        }
    }
}

void Uci::ucinewgame() {
    if (root.isBusy()) {
        io::fail_rewind(command);
        return;
    }

    root.newGame();
    root.position.setStartpos();
}

void Uci::setoption() {
    next("name");

    if (next("UCI_Chess960")) {
        next("value");

        if (next("true"))  { root.position.setChessVariant(Chess960); return; }
        if (next("false")) { root.position.setChessVariant(Orthodox); return; }

        io::fail_rewind(command);
        return;
    }

    if (next("Hash")) {
        next("value");
        setHash();
        return;
    }

}

void Uci::setHash() {
    if (root.isBusy()) {
        io::fail_rewind(command);
        return;
    }

    size_t quantity = 0;
    command >> quantity;
    if (!command) {
        io::fail_rewind(command);
        return;
    }

    io::char_type unit = 'm';
    command >> unit;

    switch (std::tolower(unit)) {
        case 't':
            quantity *= 1024;
            /* fallthrough */
        case 'g':
            quantity *= 1024;
            /* fallthrough */
        case 'm':
            quantity *= 1024;
            /* fallthrough */
        case 'k':
            quantity *= 1024;
            /* fallthrough */
        case 'b':
            break;

        default: {
            io::fail_rewind(command);
            return;
        }
    }

    root.setHash(quantity);
}

void Uci::position() {
    if (nextNothing()) {
        root.infoPosition();
        return;
    }

    if (next("startpos")) { root.position.setStartpos(); }
    if (next("fen")) { root.position.readFen(command); }

    next("moves");
    root.position.playMoves(command);
}

void Uci::go() {
    if (root.isBusy()) {
        io::fail_rewind(command);
        return;
    }

    auto whiteSide = root.position.sideOf(White);
    auto blackSide = root.position.sideOf(Black);

    UciGoLimit limit;
    limit.positionMoves = root.position;

    unsigned quantity = 0;
    while (command >> std::ws, !command.eof()) {
        if      (next("depth"))    { command >> quantity; limit.depth = std::min(quantity, MaxPly); }
        else if (next("wtime"))    { command >> limit.time[whiteSide]; }
        else if (next("btime"))    { command >> limit.time[blackSide]; }
        else if (next("winc"))     { command >> limit.inc[whiteSide]; }
        else if (next("binc"))     { command >> limit.inc[blackSide]; }
        else if (next("movestogo")){ command >> limit.movestogo; }
        else if (next("nodes"))    { command >> limit.nodes; limit.nodes = std::min(limit.nodes, static_cast<node_count_t>(NodeCountMax)); }
        else if (next("movetime")) { command >> limit.movetime; }
        else if (next("mate"))     { command >> limit.mate; }
        else if (next("ponder"))   { limit.isPonder = true; }
        else if (next("infinite")) { limit.isInfinite = true; }
        else if (next("searchmoves")) { limit.positionMoves.limitMoves(command); }
        else { io::fail(command); return; }
    }

    root.go(limit);
}

void Uci::goPerft() {
    if (root.isBusy()) {
        io::fail_rewind(command);
        return;
    }

    unsigned depth = 0;
    command >> depth;
    if (!depth) {
        io::fail_rewind(command);
        return;
    }
    depth = std::min(depth, MaxPly);

    bool isPerftDivide = false;
    if (next("divide")) {
        isPerftDivide = true;
    }

    if (nextNothing()) {
        root.goPerft(depth, isPerftDivide);
    }
}
