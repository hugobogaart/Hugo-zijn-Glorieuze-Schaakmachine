#include "cli-game.h"
#include "../Engine/position.h"
#include "cli-utils.h"

#include <iostream>
#include <sstream>
#include <string>


std::string read_user_input()
{
        std::string s;
        std::getline(std::cin, s);
        return s;
}

void uncap(char &ch) {
        if (ch >= 'A' && ch <= 'Z') {
                ch += 'a' - 'A';
        }
}

void uncap(std::string &str) {
        for (char &ch : str) {
                uncap(ch);
        }
}


void print(const PiecewiseBoard &board)
{
        // returns character associated with that tile
        auto tile_char = [&board](int rank, int file) -> char {
                auto optPiece = piece_at(board, OneSquare(rank, file));
                // return optPiece.transform(epiece_to_char).value_or(' ');
                if (!optPiece)
                        return ' ';
                return epiece_to_char(*optPiece);
        };

        std::stringstream s;
        s << '\n';

        for (int rank = 7; rank >= 0; rank--) {
                s << "+---+---+---+---+---+---+---+---+\n";
                for (int file = 0; file < 8; file++) {
                        s << "| "
                          << tile_char(rank, file)
                          << ' ';
                }
                s << "| " << rank + 1 << '\n';
        }
        s << "+---+---+---+---+---+---+---+---+\n";
        for (char file = 'a'; file < 8 + 'a'; file++) {
                s << "  " << file << ' ';
        }
        std::cout << s.str() << std::endl;
}


// returns true if set.player_color is col
// could also be both
bool is_to_turn(Color col, GameSettings set)
{
        if (col == Color::white) {
                if (set.player_colors == GameSettings::black) {
                        return false;
                }
                return true;
        }

        // col is black
        return set.player_colors != GameSettings::white;
}
