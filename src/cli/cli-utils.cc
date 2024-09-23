//
// Created by Hugo Bogaart on 18/07/2024.
//

#include "cli-utils.h"

#include "../Engine/position.h"
#include <cstring>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>

auto board2str(const PiecewiseBoard &board) -> std::string
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
        return s.str();
}

auto boards2str(const PiecewiseBoard &left, const PiecewiseBoard &right) -> std::string
{
        // const char *row_delim = "+---+---+---+---+---+---+---+---+";
        const char *arrow     = "  ->  ";

        // size_t row_size = strlen(row_delim);
        // size_t arrow_size = strlen(arrow);

        constexpr size_t row_size = 33;
        constexpr size_t arrow_size = 6;

        constexpr size_t num_rows = 8;
        constexpr size_t num_row_delims = 9;
        constexpr size_t num_files = 8;
        constexpr size_t num_file_delims = 9;
        // cols are the same

        constexpr size_t num_chars_in_boardrow = row_size + 2;
        constexpr size_t num_chars_in_line = 2 * num_chars_in_boardrow + arrow_size + 1;// \n
        constexpr size_t num_lines = num_rows + num_row_delims + 1;
        constexpr size_t num_chars = num_chars_in_line * num_lines;

        std::string output(num_chars, ' ');
        for (int line = 0; line < num_lines; line++)
                output[line * num_chars_in_line + num_chars_in_line - 1] = '\n';

        std::string strl = board2str(left);
        std::string strr = board2str(right);

        auto getlines = [](const std::string &str) -> std::vector<std::string> {
                std::vector<std::string> vec;
                for (char ch : str) {
                        if (ch == '\n') {
                                if (!vec.empty() && !vec.back().empty())
                                        vec.emplace_back();
                                continue;
                        }

                        if (vec.empty())
                                vec.emplace_back();

                        vec.back().push_back(ch);
                }
                return vec;
        };

        auto linesl = getlines(strl);
        auto linesr = getlines(strr);

        constexpr size_t r_begin = num_chars_in_boardrow + arrow_size;

        assert (num_lines == linesl.size());
        assert (num_lines == linesr.size());

        for (size_t line = 0; line < num_lines; line++) {
                std::ranges::copy(linesl[line], output.begin() + line * num_chars_in_line);
                std::ranges::copy(linesr[line], output.begin() + line * num_chars_in_line + r_begin);
        }

        // the arrow
        std::copy_n(arrow, arrow_size, output.begin() + 8 * num_chars_in_line + num_chars_in_boardrow);
        return output;
}
