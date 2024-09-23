//
// Created by Hugo Bogaart on 18/07/2024.
//


// these are some chess-command line utilities
// that are used in several command line things
#ifndef CLI_UTILS_H
#define CLI_UTILS_H

#include "../Engine/position.h"
#include <optional>


// shows a field like a chessboard-square
inline
auto num_like_board (Field num) noexcept -> std::string;

// Epiece <-> char conversions

constexpr
auto epiece_to_char(Epiece pc) noexcept -> char;

constexpr
auto char_to_piece (char ch) noexcept -> std::optional<Epiece>;

// lettered files to int
constexpr
auto char_to_file (char ch) noexcept -> std::optional<uint8_t>;

// tries to read the *ENTIRE* string as an integer, no whitespace truncation is attempted
inline
auto str_to_uint (const std::string &str) noexcept -> std::optional<uint64_t>;

// divides string into its words seperated by whitespace
inline
auto to_words (const std::string &str) noexcept -> std::vector<std::string>;

// creates a board from the fen code
// https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
inline
auto fromFen (const std::string &fen) noexcept -> std::optional<Position>;

// reads a move from uci-long algebraic notation into a Move

inline
auto fromAlgebraic (const std::string &str, const Position &position) noexcept -> std::optional<Move>;

inline
auto toAlgebraic (Move mv, Color col) noexcept -> std::string;

auto board2str(const PiecewiseBoard &board) -> std::string;
auto boards2str(const PiecewiseBoard &left, const PiecewiseBoard &right) -> std::string;


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
/// INLINE /// CONSTEXPR ///TEMPLATE DEFINITIONS
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


constexpr
auto is_whitespace (const char ch) noexcept -> bool
{
        return ch == ' ' || ch == '\t' || ch == '\n'
            || ch == '\r' || ch == '\f' || ch == '\v';
}


constexpr
auto epiece_to_char (const Epiece pc) noexcept -> char
{
        switch (pc) {
        case white_pawns:       return 'P';
        case white_bishops:     return 'B';
        case white_horses:      return 'H';
        case white_rooks:       return 'R';
        case white_queen:       return 'Q';
        case white_king:        return 'K';
        case black_pawns:       return 'p';
        case black_bishops:     return 'b';
        case black_horses:      return 'h';
        case black_rooks:       return 'r';
        case black_queen:       return 'q';
        case black_king:        return 'k';
        }
}

constexpr
auto char_to_piece (const char ch) noexcept -> std::optional<Epiece>
{
        switch (ch) {
        case 'R':
                return white_rooks;
        case 'Q':
                return white_queen;
        case 'P':
                return white_pawns;
        case 'B':
                return white_bishops;
        case 'K':
                return white_king;
        case 'N':
                return white_horses;
        case 'r':
                return black_rooks;
        case 'q':
                return black_queen;
        case 'p':
                return black_pawns;
        case 'b':
                return black_bishops;
        case 'k':
                return black_king;
        case 'n':
                return black_horses;
        default:
                return std::nullopt;
        }
}

constexpr
auto char_to_file (const char ch) noexcept -> std::optional<uint8_t>
{
        if (ch >= 'a' && ch <= 'h')
                return ch - 'a';
        return std::nullopt;
}


inline
auto str_to_uint (const std::string &str) noexcept -> std::optional<uint64_t>
{
        if (str.empty())
                return std::nullopt;

        auto digit = [](const char ch) -> std::optional<uint64_t> {
                if (ch >= '0' && ch <= '9')
                        return ch - '0';
                return std::nullopt;
        };

        uint64_t res = 0;
        for (const char ch : str) {
                std::optional<uint64_t> new_digit = digit(ch);
                if (!new_digit.has_value()) {
                        // not  digit
                        return std::nullopt;
                }
                res = 10 * res + new_digit.value();
        }
        return res;
}

inline
auto num_like_board (const Field num) noexcept -> std::string
{

        std::string s = "\n";
        char ch;
        for (int rank = 7; rank >= 0; rank--) {
                for (int file = 0; file < 8; file++) {
                        ch = (num & 1ull << (8 * rank + file)) ? '1' : '0';
                        s += ch;
                        s += ' ';
                }
                s += '\n';
        }
        return s;
}


inline
auto to_words (const std::string &str) noexcept -> std::vector<std::string>
{
        std::vector<std::string> words;
        const char *p = str.c_str();

        // turns string into words
        bool begin_new = true;
        while (p < str.cend().base()) {
                if (is_whitespace(*p)) {
                        p++;
                        begin_new = true;
                } else {
                        if (begin_new) {
                                words.emplace_back();
                                begin_new = false;
                        }
                        words.back().push_back(*p);
                        p++;
                }
        }
        return words;
}

inline
auto fromFen (const std::string &fen) noexcept -> std::optional<Position>
{
        const auto fen_parts = to_words(fen);
        if (fen_parts.size() < 4)
                return std::nullopt;

        Position board = empty_position;

        // 1 - piece placement data
        const std::string &board_str = fen_parts[0];

        int rank = 7, file = 0;
        for (char ch : board_str) {
                if (ch == '/') {
                        rank--;
                        file = 0;
                        continue;
                } else if (ch >= '1' && ch <= '8') {
                        // empty squares
                        file += ch - '0';
                        continue;
                }

                const std::optional<Epiece> optpc = char_to_piece(ch);
                if (optpc.has_value()) {
                        // add piece to the board
                        const OneSquare sq = OneSquare(rank, file);
                        board.piece_field(optpc.value()) |= sq;
                }
                file++;
        }

        // 2 - color
        char cchar = fen_parts[1][0];
        if (cchar == 'w')
                board.meta.active = Color::white;
        else if (cchar == 'b')
                board.meta.active = Color::black;
        else
                return std::nullopt;

        // 3 - castling
        std::string castle = fen_parts[2];

        // no castling by default
        board.meta.disallow_king_castle<Color::white>();
        board.meta.disallow_king_castle<Color::black>();
        board.meta.disallow_queen_castle<Color::white>();
        board.meta.disallow_queen_castle<Color::black>();

        for (char ch : castle) {
                if (ch == '-') {
                        break;
                }
                if (ch == 'K') {
                        // board.meta.white_king_castle = true;
                        board.meta.castle_rights |= MetaData::white_kingside;
                } else if (ch == 'k') {
                        // board.meta.black_king_castle = true;
                        board.meta.castle_rights |= MetaData::black_kingside;
                } else if (ch == 'Q') {
                        // board.meta.white_queen_castle = true;
                        board.meta.castle_rights |= MetaData::white_queenside;
                } else if (ch == 'q') {
                        // board.meta.black_queen_castle = true;
                        board.meta.castle_rights |= MetaData::black_queenside;
                } else {
                        // bad character
                        return std::nullopt;
                }
        }

        // 4 - en passant

        const std::string &eps = fen_parts[3];
        if (eps.empty()) // should never happen anyway
                return std::nullopt;

        // either eps is "-" or something like "f3"
        if (eps == "-") {
                board.meta.set_pawn_2fwd(8);    // none
        } else if (eps.size() == 2) {
                std::optional<uint8_t> optfile = char_to_file(eps[0]);
                if (!optfile.has_value()) {
                        // not a non- en passent but an error, invalid input
                        return std::nullopt;
                }
                board.meta.set_pawn_2fwd(optfile.value());
        } else {
                // error as well
                return std::nullopt;
        }

        // sometimes the next 2 are missing
        // in that case we don't care

        // 5 - Halfmove clock
        if (fen_parts.size() >= 5) {
                const std::string &hm_str = fen_parts[4];
                std::optional<uint64_t> num_moves = str_to_uint(hm_str);
                if (num_moves.has_value() == false)
                        return std::nullopt;
                board.meta.passive_move_counter = num_moves.value();
        } else {
                board.meta.passive_move_counter = 0;
        }
        // 6 - FullMove number, number of total moves played in the entire game

        // we actually don't care about this
        return board;
}

inline
auto fromAlgebraic (const std::string &str, const Position &position) noexcept -> std::optional<Move>
{
        // nullmove is "0000"
        // don't care

        // very simple format
        // <origin file> <origin rank> <dest file> <dest ranl> (<pawn prommotion>)

        if (str.size() < 4 || str.size() > 5)
                return std::nullopt;

        // reads a character as a file-index, if possible
        auto to_rank = [](char ch) -> std::optional<int> {
                if (ch >= '1' && ch <= '8')
                        return ch - '1';
                return std::nullopt;
        };



        std::optional<uint8_t> ffrom, fto, rfrom, rto;
        ffrom = char_to_file(str[0]);
        rfrom = to_rank(str[1]);
        fto   = char_to_file(str[2]);
        rto   = to_rank(str[3]);

        if (!(ffrom && rfrom && fto && rto)) {
                return std::nullopt;
        }

        OneSquare from(*rfrom, *ffrom);
        OneSquare to(*rto, *fto);


        if (str.size() == 5) {
                // only for promotion are there five characters
                switch (str[4]) {
                case 'q':
                        return Move(from, to, Move::Promotion::queen_promo);
                case 'r':
                        return Move(from, to, Move::Promotion::rook_promo);
                case 'n':
                        return Move(from, to, Move::Promotion::horse_promo);
                case 'b':
                        return Move(from, to, Move::Promotion::bishop_promo);
                default:
                        return std::nullopt;  // bad char
                }
        }

        // now just the special cases
        // castling is input as just a 2-tile king move

        // do we move a king?
        Color col = position.meta.active;

        if (position.has_at<white_king>(from) || position.has_at<black_king>(from)) {
                // are the two squares really the two relevant king squares?
                const Field castle_from = col == Color::white ? white_king_start : black_king_start;
                const Field castle_to_kingside = col == Color::white ? white_king_kingside_to : black_king_kingside_to;
                const Field castle_to_queenside = col == Color::white ? white_king_queenside_to : black_king_queenside_to;

                if (from == castle_from && to == castle_to_kingside)
                        return king_castle_mv;
                else if (from == castle_from && to == castle_to_queenside)
                        return queen_castle_mv;
        }

        // en passant flag if
        //  * we move a pawn
        //  * the pawn changes file
        //  * there is no piece there
        // -> trigger the en passant flag
        // if the move is invalid, it will be  rejected anyway since from_square and to_square are used as well

        const Field occ_hostile = col == Color::white ? position.get_occupation<Color::black>() : position.get_occupation<Color::white>();
        const bool en_passant = (position.has_at<white_pawns>(from) || position.has_at<black_pawns>(from)) &&
                                *ffrom != *fto &&
                                (occ_hostile & to) == 0;
        if (en_passant)
                return Move(from, to, Move::EnPassant);

        return Move(from, to);
}

inline
auto toAlgebraic (Move mv, Color col) noexcept -> std::string
{
        bool is_white = col == Color::white;
        std::string str;
        str.resize(4);

        // if there is promotion, the same exact thing happens as a normal move,
        // except there is also a 5th character; the piece to promote to
        switch (mv.get_special()) {
        case Move::Special::castle:
                if (mv.get_castle_type() == Move::CastleType::queenside)
                        str = is_white ? "e1c1" : "e8c8";
                else // kingside
                        str = is_white ? "e1g1" : "e8g8";
                break;
        case Move::Special::promotion:
                str.resize(5);
                if (mv.get_promotion() == Move::Promotion::queen_promo)
                        str[4] = 'q';
                else if (mv.get_promotion() == Move::Promotion::rook_promo)
                        str[4] = 'r';
                else if (mv.get_promotion() == Move::Promotion::horse_promo)
                        str[4] = 'n';
                else
                        str[4] = 'b';
                [[fallthrough]];
        case Move::Special::none:
        case Move::Special::en_passant:
                str[0] = mv.from_file() + 'a';
                str[1] = mv.from_rank() + '1';
                str[2] = mv.to_file()   + 'a';
                str[3] = mv.to_rank()   + '1';
                break;
        }
        return str;
}


#endif //CLI_UTILS_H
