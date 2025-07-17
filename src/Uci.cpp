#include "Uci.hpp"
#include "Milliseconds.hpp"
#include "SearchLimit.hpp"

namespace {
    ostream& uci_error(ostream& err, io::istream& context) {
        return err << "parsing error: " << context.rdbuf() << '\n';
    }
}

Uci::Uci (ostream& out):
    positionFen{},
    info(out, positionFen),
    searchControl(info)
{
    ucinewgame();
}

void Uci::operator() (io::istream& in, ostream& err) {
    for (std::string currentLine; std::getline(in, currentLine); ) {
        command.clear(); //clear state from the previous command
        command.str(std::move(currentLine));
        command >> std::ws;

        if      (next("go"))        { go(); }
        else if (next("position"))  { position(); }
        else if (next("stop"))      { searchControl.stop(); }
        else if (next("isready"))   { searchControl.isready(); }
        else if (next("setoption")) { setoption(); }
        else if (next("set"))       { setoption(); }
        else if (next("ucinewgame")){ ucinewgame(); }
        else if (next("uci"))       { searchControl.uciok(); }
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
    if (searchControl.isBusy()) {
        io::fail_rewind(command);
        return;
    }

    searchControl.newGame();
    positionFen.setStartpos();
}

void Uci::setoption() {
    next("name");

    if (next("UCI_Chess960")) {
        next("value");

        if (next("true"))  { positionFen.setChessVariant(Chess960); return; }
        if (next("false")) { positionFen.setChessVariant(Orthodox); return; }

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
    if (searchControl.isBusy()) {
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

    searchControl.setHash(quantity);
}

void Uci::position() {
    if (nextNothing()) {
        searchControl.infoPosition();
        return;
    }

    if (next("startpos")) { positionFen.setStartpos(); }
    if (next("fen")) { positionFen.readFen(command); }

    next("moves");
    positionFen.playMoves(command);
}

void Uci::go() {
    if (searchControl.isBusy()) {
        io::fail_rewind(command);
        return;
    }

    auto whiteSide = positionFen.sideOf(White);
    auto blackSide = positionFen.sideOf(Black);

    SearchLimit limit;
    limit.positionMoves = positionFen;

    while (command >> std::ws, !command.eof()) {
        if      (next("depth"))    { command >> limit.depth; limit.depth = std::min(limit.depth, static_cast<ply_t>(MaxDepth)); }
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

    searchControl.go(limit);
}

void Uci::goPerft() {
    if (searchControl.isBusy()) {
        io::fail_rewind(command);
        return;
    }

    unsigned depth = 0;
    command >> depth;
    if (!depth) {
        io::fail_rewind(command);
        return;
    }
    depth = std::min(depth, +MaxDepth);

    bool isPerftDivide = false;
    if (next("divide")) {
        isPerftDivide = true;
    }

    if (nextNothing()) {
        searchControl.goPerft(depth, isPerftDivide);
    }
}
