/*
 * declares the EngineBoard class used to play chess on
 * as well as some associated functionality
 *
 * this file also contains the function that generates moves efficiently
 * template specialization in associated .cpp file
 *
 */

// todo isolate the EngineBoardGame in the cli


#ifndef BOT_ENGINE_BOARD_H
#define BOT_ENGINE_BOARD_H

#include "gen-defs.h"
#include "bitfield.h"

#include <vector>
#include <algorithm>
#include <optional>

#include <cassert>

constexpr CalculationType horses_ct = CalculationType::lookup_table;
constexpr CalculationType kings_ct  = CalculationType::lookup_table;

// the starting board
constexpr Field start_board[12] = {
    0b0000000000000000000000000000000000000000000000000000000000010000,     // rank 0 file 4            white king
    0b0000000000000000000000000000000000000000000000000000000000001000,     // rank 0 file 3            white queen
    0b0000000000000000000000000000000000000000000000000000000001000010,     // rank 0 file 1, 6         white horses
    0b0000000000000000000000000000000000000000000000000000000010000001,     // rank 0 file 0, 7         white rooks
    0b0000000000000000000000000000000000000000000000000000000000100100,     // rank 0 file 2, 5         white bishops
    0b0000000000000000000000000000000000000000000000001111111100000000,     // rank 1                   white pawns
    0b0001000000000000000000000000000000000000000000000000000000000000,     // rank 7 file 4            black king
    0b0000100000000000000000000000000000000000000000000000000000000000,     // rank 7 file 3            black queen
    0b0100001000000000000000000000000000000000000000000000000000000000,     // rank 7 file 1, 6         black horses
    0b1000000100000000000000000000000000000000000000000000000000000000,     // rank 7 file 0, 7         black rooks
    0b0010010000000000000000000000000000000000000000000000000000000000,     // rank 7 file 2, 5         black bishops
    0b0000000011111111000000000000000000000000000000000000000000000000      // rank 6                   black pawns
};


// start and destination locations of castling-related pieces

constexpr Field white_queen_rook    = OneSquare(0, 0),       // location of white queen-side rook
                white_queen_rook_to = OneSquare(0, 3),
                white_king_rook     = OneSquare(0, 7),
                white_king_rook_to  = OneSquare(0, 5),
                black_queen_rook    = OneSquare(7, 0),
                black_queen_rook_to = OneSquare(7, 3),
                black_king_rook     = OneSquare(7, 7),
                black_king_rook_to  = OneSquare(7, 5);


constexpr Field white_king_start        = OneSquare(0, 4),
                white_king_queenside_to = OneSquare(0, 2),   // in a queenside castle the king goes here
                white_king_kingside_to  = OneSquare(0, 6),
                black_king_start        = OneSquare(7, 4),
                black_king_queenside_to = OneSquare(7, 2),   // in a queenside castle the king goes here
                black_king_kingside_to  = OneSquare(7, 6);

// when white wants to castle, this is the zone that must be unobstructed
constexpr Field white_castle_queen_freezone = OneSquare(0, 1) | OneSquare(0, 2) | OneSquare(0, 3);
constexpr Field white_castle_king_freezone  = OneSquare(0, 5) | OneSquare(0, 6);
constexpr Field black_castle_queen_freezone = white_castle_queen_freezone << 7 * 8;
constexpr Field black_castle_king_freezone  = white_castle_king_freezone << 7 * 8;

// when white wants to castle, this is the zone that must not be under attack
constexpr Field white_castle_queen_safezone = white_castle_queen_freezone << 1;
constexpr Field white_castle_king_safezone  = white_king_start | white_castle_king_freezone;
constexpr Field black_castle_queen_safezone = white_castle_queen_safezone << 7 * 8;
constexpr Field black_castle_king_safezone = white_castle_king_safezone << 7 * 8;


// just to type the pieces
enum Epiece : uint8_t {
        white_king = 0,
        white_queen,
        white_horses,
        white_rooks,
        white_bishops,
        white_pawns,
        black_king,
        black_queen,
        black_horses,
        black_rooks,
        black_bishops,
        black_pawns
};


[[nodiscard]]
constexpr Color get_color(Epiece piece)
{
        switch (piece) {
        case white_king:
        case white_queen:
        case white_rooks:
        case white_bishops:
        case white_horses:
        case white_pawns:     return Color::white;
        default:              return Color::black;
        }
}

// prints out a field formatted like a chess board

/*
 * data of the board that is not encoded on the board, like castling/en passant right
 * threefold repetition is NOT used
 */

struct MetaData
{
        // counts half moves (1 per player) NOT full moves (one for a move from both players)
        uint8_t passive_move_counter;   // to 50 passive moves (50 per player), or 100 moves total


        // pawn_2_fwd_file is set to some value between 0 and 7 if tha last color has just leaped 2 tiles forward with that pawn
        // 4 bits necessary to describe one of the 8 files, or none
        // value of 8 means no en passant
        uint8_t pawn_2_fwd_file : 4;

        // castling right

        enum CastleType {
                white_queenside = 1 << 3,
                black_queenside = 1 << 2,
                white_kingside  = 1 << 1,
                black_kingside  = 1 << 0
        };

        uint8_t castle_rights : 4;

        /*
        bool white_queen_castle : 1;
        bool white_king_castle  : 1;
        bool black_king_castle  : 1;
        bool black_queen_castle : 1;
        */

        // color to move
        Color active            : 1;

        // unititialized
        constexpr MetaData() = default;

        template <Color col>
        [[nodiscard]]
        constexpr
        bool get_queen_castle() const noexcept;

        template <Color col>
        [[nodiscard]]
        constexpr
        bool get_king_castle() const noexcept;

        // returns the file on which the pawn of the last move has moved 2 ranks
        // or 8 if this did not happen

        [[nodiscard]]
        constexpr
        uint8_t pawn2fwd_file() const noexcept;

        // specifies in the metadata that the last color has just moved their pawn
        // two squares forward in this file, or file==8 if they did not
        constexpr
        void set_pawn_2fwd(uint8_t file) noexcept;

        constexpr
        void reset_passive_move_counter() noexcept;

        constexpr
        void inc_passive_move_counter() noexcept;

        template <Color col>
        constexpr
        void disallow_queen_castle() noexcept;

        template <Color col>
        constexpr
        void disallow_king_castle() noexcept;
};

constexpr
bool operator==(const MetaData &m1, const MetaData &m2);

// the metadata for the starting board
// the constexpr varible start_meta is defined


// highly efficient board format
// only saves the location of the pieces
struct PiecewiseBoard {
        // rank major order, so rank 2 file 3 is index 19
        // zero indexed files and ranks
        // idx = rank * 8 + file

        // the enum values are the indices as well, so board[white_rooks] is the white rooks
        Field board[12];

        // only copies are made anyway
        constexpr PiecewiseBoard() = default;
        constexpr PiecewiseBoard(const PiecewiseBoard &other) = default;
        constexpr auto operator= (const PiecewiseBoard &other) -> PiecewiseBoard &;

        // sets the board to the starting position
        constexpr
        void to_start();


        // returns a field type that contains all squares that color col attacks
        // does not include the squares where its own pieces are standing in the way

        template <Color col>
        [[nodiscard, gnu::pure]]
        constexpr
        Field attack_map() const;

        // returns a field type that contains all squares that color col attacks
        // also includes its own pieces that are defended
        template <Color col>
        [[nodiscard, gnu::pure]]
        constexpr
        Field defend_map() const;

        // returns a field with a 1 at each point a piece of color col stands
        template <Color col>
        [[nodiscard, gnu::pure]]
        constexpr
        Field get_occupation() const;

        // returns true if the King of color col is in check
        template <Color col>
        [[nodiscard]]
        constexpr
        bool in_check() const;

        // returns true if the piece is on that rank and file
        template <Epiece piece>
        [[nodiscard]]
        constexpr
        bool is_on(uint8_t rank, uint8_t file) const;

        // these functions return the field of piece of color col
        template <Color col>
        constexpr
        Field &pawns();

        template <Color col>
        constexpr
        Field &king();

        template <Color col>
        constexpr
        Field &queen();

        template <Color col>
        constexpr
        Field &horses();

        template <Color col>
        constexpr
        Field &rooks();

        template <Color col>
        constexpr
        Field &bishops();

        template <Color col>
        [[nodiscard]]
        constexpr
        Field pawns() const;

        template <Color col>
        [[nodiscard]]
        constexpr
        Field king() const;

        template <Color col>
        [[nodiscard]]
        constexpr
        Field queen() const;

        template <Color col>
        [[nodiscard]]
        constexpr
        Field horses() const;

        template <Color col>
        [[nodiscard]]
        constexpr
        Field rooks() const;

        template <Color col>
        [[nodiscard]]
        constexpr
        Field bishops() const;

        // like the templated accesses, but with a variable instead
        constexpr
        auto piece_field (Epiece epc) -> Field &;

        constexpr
        auto piece_field (Epiece epc) const -> Field;

        // returns true if the board has this piece at that field

        template <Epiece pc>
        [[nodiscard]]
        constexpr
        auto has_at (Field f) const -> bool;


        // if f is a bitmap field, all the bits of f
        // will set the bits of the fields of color col to 0
        // so if a white horse goes to field "white_horse_to"
        // we can do set_all_0<black>(white_horse_to) to capture any black piece
        // without checking which one is there (if any)
        template<Color col>
        constexpr
        void set_all_0(Field f);
};


constexpr
bool boardEq(const PiecewiseBoard &b1, const PiecewiseBoard &b2);


// also contains metadata
struct Position : public PiecewiseBoard {
        MetaData meta;

        constexpr Position() = default;
        constexpr Position(const Position &other);

        /*
         * copies the board, but not the metadata
         */
        constexpr void copy_board(const Position &other) noexcept;

        // copy operator copies everything
        constexpr
        Position &operator=(const Position &other);

};

// the variables start_epwboard, empty_epwboard are defined

constexpr
auto operator== (const Position &left, const Position &right) -> bool
{
        return left.meta == right.meta && boardEq(left, right);
}


/*
// the states the game can find itself in
enum GameState {
        none,
        white_win,
        black_win,
        stalemate,
        remise
};

// the class that encapsulates an entire chess game
struct EngineBoardGame {
        // top element is the current board
        // these boards contain the metadata as well
        std::vector<const Position> boards;

        EngineBoardGame();
        EngineBoardGame(const EngineBoardGame &other) = default;
        EngineBoardGame(EngineBoardGame &&other) noexcept = default;
        EngineBoardGame &operator=(const EngineBoardGame &other) = default;
        EngineBoardGame &operator=(EngineBoardGame &&other) noexcept = default;

        // returns the current board
        [[nodiscard]]
        const Position &current() const noexcept;

        // add a board to the game
        // no checking is done
        void add_board(const Position &new_board);

        // takes back a move
        void take_back();

        // returns the color that may make a move
        [[nodiscard]] Color turn() const;

        [[nodiscard]] GameState get_state() const;

        // to starting game
        constexpr
        void reset();
};

typedef EngineBoardGame ChessGame;
*/

// enough information to contain a move
// supposed to be used for engine
// not enough information is saved to later take back the move
// the only move class we will need anyway
struct Move
{

        // the rank and file is stored as the offset
        // so OneSquare from = 1ull << from_shift
        // only 6 bits are needed for the 2^6 == 64 possible bits

        // uint8_t from_shift : 6;

        // these two things are mutually exclusive in a move anyway
        // so all these things fit in 2 bits


        enum Promotion : uint8_t {
                rook_promo, queen_promo, horse_promo, bishop_promo,
        };

        enum CastleType : uint8_t {
                kingside, queenside
        };

        enum Special : uint8_t {
                none, castle, en_passant, promotion
        };

        // uint8_t promotion_or_castle : 2;

        // uint8_t to_shift : 6;

        // special : 2;


        Move () = default;

        // normal move
        constexpr
        Move (OneSquare from, OneSquare to)
        {
                set_from_shift(square_to_shift(from));
                set_to_shift(square_to_shift(to));
                set_special(none);
        }

        // move with promotion
        constexpr
        Move (OneSquare from, OneSquare to, Promotion promo)
        {
                set_from_shift(square_to_shift(from));
                set_to_shift(square_to_shift(to));
                set_promotion(promo);
                set_special(promotion);
        }

        constexpr
        Move (CastleType castle_type)
        {
                set_castle_type(castle_type);
                set_special(castle);
        }

        // helper struct
private:
        struct EnPassant_ {};
public:
        static constexpr EnPassant_ EnPassant{};

        // used for en passant
        constexpr
        Move (OneSquare from, OneSquare to, EnPassant_)
        {
                set_from_shift(square_to_shift(from));
                set_to_shift(square_to_shift(to));
                set_special(en_passant);
        }

        // the castling moves are predefined as king_castle_mv and queen_castle_mv

        constexpr auto from_square () const -> OneSquare {return square_from_shift(get_from_shift());}
        constexpr auto to_square () const -> OneSquare {return square_from_shift(get_to_shift());}
        constexpr auto from_file () const -> uint8_t {return shift_to_file(get_from_shift());}
        constexpr auto from_rank () const -> uint8_t {return shift_to_rank(get_from_shift());}
        constexpr auto to_file () const -> uint8_t {return shift_to_file(get_to_shift());}
        constexpr auto to_rank () const -> uint8_t {return shift_to_rank(get_to_shift());}

private:
        uint16_t data = 0;

        constexpr void set_from_shift(uint8_t fs)
        {
                // set first 6 bits of data to the first six bits of fs
                constexpr uint16_t first_six_bits = 0b111111;
                data &= ~first_six_bits;
                data ^= fs & first_six_bits;
        }

        constexpr void set_to_shift(uint8_t fs)
        {
                // set first 6 bits of data to the first six bits of fs
                constexpr uint16_t second_six_bits = 0b111111 << 6;
                data &= ~second_six_bits;
                data ^= (fs & 0b111111) << 6;
        }
        constexpr void set_castle_type (CastleType ct)
        {
                // bit 13 and 14
                constexpr uint16_t castle_type_bits = 0b11 << 12;
                data &= ~castle_type_bits;
                data ^= ct << 12;
        }
        constexpr void set_promotion (Promotion pr)
        {
                // bit 13 and 14
                constexpr uint16_t promotion_bits = 0b11 << 12;
                data &= ~promotion_bits;
                data ^= pr << 12;
        }
        constexpr void set_special (Special sp)
        {
                // bit 15 and 16
                constexpr uint16_t special_bits = 0b11 << 14;
                data &= ~special_bits;
                data ^= sp << 14;
        }
        constexpr uint8_t get_from_shift() const
        {
                constexpr uint16_t first_six_bits = 0b111111;
                return data & first_six_bits;
        }

        constexpr uint8_t get_to_shift() const
        {
                constexpr uint16_t second_six_bits = 0b111111 << 6;
                return (data & second_six_bits) >> 6;
        }
public:
        constexpr CastleType get_castle_type () const
        {
                constexpr uint16_t castle_type_bits = 0b11 << 12;
                return static_cast<CastleType>((data & castle_type_bits) >> 12);
        }
        constexpr Promotion get_promotion () const
        {
                // bit 13 and 14
                constexpr uint16_t promotion_bits = 0b11 << 12;
                return static_cast<Promotion>((data & promotion_bits) >> 12);
        }
        constexpr Special get_special () const
        {
                // bit 15 and 16
                constexpr uint16_t special_bits = 0b11 << 14;
                return static_cast<Special>((data & special_bits) >> 14);
        }

        constexpr friend
        auto operator== (Move l, Move r);

        friend auto test_nodegen() -> void;

        template <Color col>
        friend auto perft_compare_col (const Position &position, int ply) -> size_t;
};

constexpr
auto operator== (Move l, Move r);

constexpr Move king_castle_mv(Move::CastleType::kingside);
constexpr Move queen_castle_mv(Move::CastleType::queenside);

// returns field with all places the horse can jump to
inline
Field get_horse_jumps(OneSquare sq);


// returns precisely the tiles the king can go to on an empty board
// assumes there is only one king

inline
Field get_king_area(OneSquare sq);



// returns the piece that is located on field idx
constexpr
auto piece_at(const PiecewiseBoard &board, OneSquare point) -> std::optional<Epiece>;


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
///  DEFINITIONS
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

constexpr
auto piece_at(const PiecewiseBoard &board, OneSquare point) -> std::optional<Epiece>
{
        // assert(bit_count(idx) == 1);

        if (board.rooks<Color::white>() & point) {
                return white_rooks;
        } else if (board.queen<Color::white>() & point) {
                return white_queen;
        } else if (board.pawns<Color::white>() & point) {
                return white_pawns;
        } else if (board.bishops<Color::white>() & point) {
                return white_bishops;
        } else if (board.king<Color::white>() & point) {
                return white_king;
        } else if (board.horses<Color::white>() & point) {
                return white_horses;
        } else if (board.rooks<Color::black>() & point) {
                return black_rooks;
        } else if (board.queen<Color::black>() & point) {
                return black_queen;
        } else if (board.pawns<Color::black>() & point) {
                return black_pawns;
        } else if (board.bishops<Color::black>() & point) {
                return black_bishops;
        } else if (board.king<Color::black>() & point) {
                return black_king;
        } else if (board.horses<Color::black>() & point) {
                return black_horses;
        } else {
                return std::nullopt;
        }
}


constexpr
bool boardEq(const PiecewiseBoard &b1, const PiecewiseBoard &b2)
{
        return std::equal(b1.board, b1.board + 12, b2.board);
}


template <Color col>
[[nodiscard]]
constexpr
bool MetaData::get_queen_castle() const noexcept
{
        // return col == Color::white ? white_queen_castle : black_queen_castle;
        return castle_rights & (col == Color::white ? white_queenside : black_queenside);
}

template <Color col>
[[nodiscard]]
constexpr
bool MetaData::get_king_castle() const noexcept
{
        // return col == Color::white ? white_king_castle : black_king_castle;
        return castle_rights & (col == Color::white ? white_kingside : black_kingside);
}


[[nodiscard]]
constexpr
uint8_t MetaData::pawn2fwd_file() const noexcept
{
        return pawn_2_fwd_file;
}

constexpr
void MetaData::set_pawn_2fwd(const uint8_t file) noexcept
{
        pawn_2_fwd_file = file;
}

constexpr
void MetaData::reset_passive_move_counter() noexcept
{
        passive_move_counter = 0;
}

constexpr
void MetaData::inc_passive_move_counter() noexcept
{
        passive_move_counter++;
}

template <Color col>
constexpr
void MetaData::disallow_queen_castle() noexcept
{
        if constexpr (col == Color::white) {
                // white_queen_castle = false;
                castle_rights &= ~white_queenside;
        } else {
                // black_queen_castle = false;
                castle_rights &= ~black_queenside;
        }
}


template <Color col>
constexpr
void MetaData::disallow_king_castle() noexcept
{
        if constexpr (col == Color::white) {
                // white_king_castle = false;
                castle_rights &= ~white_kingside;
        } else {
                // black_king_castle = false;
                castle_rights &= ~black_kingside;
        }
}


constexpr
bool operator==(const MetaData &m1, const MetaData &m2)
{
        /*
        return  m1.black_queen_castle == m2.black_queen_castle &&
                m1.white_queen_castle == m2.white_queen_castle &&
                m1.black_king_castle == m2.black_king_castle &&
                m1.white_king_castle == m2.white_king_castle &&
                m1.pawn_2_fwd_file == m2.pawn_2_fwd_file &&
                m1.passive_move_counter == m2.passive_move_counter;
                */
        return m1.castle_rights == m2.castle_rights &&
                m1.pawn_2_fwd_file == m2.pawn_2_fwd_file &&
                m1.passive_move_counter == m2.passive_move_counter;
}

constexpr
auto PiecewiseBoard::operator= (const PiecewiseBoard &other) -> PiecewiseBoard &
{
        // does not handle self assignment well. Too bad!
        std::copy_n(other.board, 12, board);
        return *this;
}

// sets the board to the starting position
constexpr
void PiecewiseBoard::to_start()
{
        std::copy_n(start_board,12, board);
}

template <Color col>
constexpr
Field &PiecewiseBoard::pawns()
{
        return col == Color::white ? board[white_pawns] : board[black_pawns];
}

template <Color col>
constexpr
Field &PiecewiseBoard::king()
{
        return col == Color::white ? board[white_king] : board[black_king];
}

template <Color col>
constexpr
Field &PiecewiseBoard::queen()
{
        return col == Color::white ? board[white_queen] : board[black_queen];
}

template <Color col>
constexpr
Field &PiecewiseBoard::horses()
{
        return col == Color::white ? board[white_horses] : board[black_horses];
}

template <Color col>
constexpr
Field &PiecewiseBoard::rooks()
{
        return col == Color::white ? board[white_rooks] : board[black_rooks];
}

template <Color col>
constexpr
Field &PiecewiseBoard::bishops()
{
        return col == Color::white ? board[white_bishops] : board[black_bishops];
}

template <Color col>
[[nodiscard]]
constexpr
Field PiecewiseBoard::pawns() const
{
        return col == Color::white ? board[white_pawns] : board[black_pawns];
}

template <Color col>
[[nodiscard]]
constexpr
Field PiecewiseBoard::king() const
{
        return col == Color::white ? board[white_king] : board[black_king];
}

template <Color col>
[[nodiscard]]
constexpr
Field PiecewiseBoard::queen() const
{
        return col == Color::white ? board[white_queen] : board[black_queen];
}

template <Color col>
[[nodiscard]]
constexpr
Field PiecewiseBoard::horses() const
{
        return col == Color::white ? board[white_horses] : board[black_horses];
}

template <Color col>
[[nodiscard]]
constexpr
Field PiecewiseBoard::rooks() const
{
        return col == Color::white ? board[white_rooks] : board[black_rooks];
}

template <Color col>
[[nodiscard]]
constexpr
Field PiecewiseBoard::bishops() const
{
        return col == Color::white ? board[white_bishops] : board[black_bishops];
}

constexpr
auto PiecewiseBoard::piece_field (Epiece epc) -> Field &
{
        return board[epc];
}

constexpr
auto PiecewiseBoard::piece_field (Epiece epc) const -> Field
{
        return board[epc];
}

template <Epiece pc>
[[nodiscard]]
constexpr
auto PiecewiseBoard::has_at (Field f) const -> bool
{
        return board[pc] & f;
}

// returns all squares that contain a white piece
template<>
constexpr
Field PiecewiseBoard::get_occupation<Color::white>() const
{
        return board[0] | board[1] | board[2] | board[3] | board[4] | board[5];
}

// returns all squares that contain a black piece
template<>
constexpr
Field PiecewiseBoard::get_occupation<Color::black>() const
{
        return board[6] | board[7] | board[8] | board[9] | board[10] | board[11];
}



template <Color col>
constexpr
Field PiecewiseBoard::attack_map() const
{
        const Field friendly_occ = get_occupation<col>();
        const Field hostile_occ  = get_occupation<!col>();

        const Field all = friendly_occ | hostile_occ;

        // the returned field that attacked squares will be added onto
        Field attack = 0;

        // pawns can attack 1 diagonal to the front, if there are enemies there
        // pawns can all be done at once
        // white pawns attack to the TOP, black to the BOTTOM

        if constexpr (col == Color::white) {
                attack |= shifted<northEast>(pawns<col>());
                attack |= shifted<northWest>(pawns<col>());
        } else /* black */ {
                attack |= shifted<southEast>(pawns<col>());
                attack |= shifted<southWest>(pawns<col>());
        }

        const Field straight_attackers = rooks<col>()   | queen<col>();
        const Field diagonal_attackers = bishops<col>() | queen<col>();

        for (const auto sq : all_squares) {
                // for efficiency purposes, we continue early if there is no friendly piece there
                // todo refactor with some continues for more efficiency
                if (!(sq & friendly_occ))
                        continue;

                if (sq & straight_attackers) {
                        attack |= get_weakly_blocked_straights(sq, all);
                }
                if (sq & diagonal_attackers) {
                        attack |= get_weakly_blocked_diagonals(sq, all);
                } else if (sq & horses<col>()) {
                        attack |= get_horse_jumps(sq);
                } else if (sq & king<col>()) {
                        attack |= get_king_area(sq);
                }
        }
        // we can not attack our own pieces, so we remove those tiles
        return attack & ~friendly_occ;
}

// returns a field type that contains all squares that color col attacks
// also includes its own pieces that are defended
template <Color col>
[[nodiscard, gnu::pure]]
constexpr
Field PiecewiseBoard::defend_map() const
{
        const Field friendly_occ = get_occupation<col>();
        const Field hostile_occ  = get_occupation<!col>();

        const Field all = friendly_occ | hostile_occ;

        // the returned field that attacked squares will be added onto
        Field defend = 0;

        // pawns can attack 1 diagonal to the front, if there are enemies there
        // pawns can all be done at once
        // white pawns attack to the TOP, black to the BOTTOM

        if constexpr (col == Color::white) {
                defend |= shifted<northEast>(pawns<col>());
                defend |= shifted<northWest>(pawns<col>());
        } else /* black */ {
                defend |= shifted<southEast>(pawns<col>());
                defend |= shifted<southWest>(pawns<col>());
        }

        const Field straight_attackers = rooks<col>()   | queen<col>();
        const Field diagonal_attackers = bishops<col>() | queen<col>();

        for (const auto sq : all_squares) {
                if (!(sq & friendly_occ))
                        continue;

                if (sq & straight_attackers) {
                        defend |= get_weakly_blocked_straights(sq, all);
                }
                if (sq & diagonal_attackers) {
                        defend |= get_weakly_blocked_diagonals(sq, all);
                } else if (sq & horses<col>()) {
                        defend |= get_horse_jumps(sq);
                } else if (sq & king<col>()) {
                        defend |= get_king_area(sq);
                }
        }
        return defend;
}

//todo test
// true if color col is in check in this position
template <Color col>
constexpr
bool PiecewiseBoard::in_check() const
{
        // return king<col>() & attack_map<!col>();
#if 0
        constexpr bool is_white = col == Color::white;
        // return king<col>() & attack_map<!col>();
        const OneSquare k = OneSquare_unsafe(king<col>());
        const Field friendly_occ = get_occupation<col>();
        const Field hostile_occ  = get_occupation<!col>();
        const Field all = friendly_occ | hostile_occ;
        const Field weak_straights = get_weakly_blocked_straights(k, all);
        if (weak_straights & (rooks<!col>() | queen<!col>())) {
                if ((king<col>() & attack_map<!col>()) == false)
                        throw std::runtime_error("wrong check calc");
                return true;
        }
        const Field weak_diags = get_weakly_blocked_diagonals(k, all);
        if (weak_diags & (bishops<!col>() | queen<!col>())) {
                if ((king<col>() & attack_map<!col>()) == false)
                        throw std::runtime_error("wrong check calc");
                return true;
        }
        if (get_horse_jumps(k) & horses<!col>()) {
                if ((king<col>() & attack_map<!col>()) == false)
                        throw std::runtime_error("wrong check calc");
                return true;
        }

        constexpr Direction pawn_dir1 = is_white ? northEast : southEast;
        constexpr Direction pawn_dir2 = is_white ? northWest : southWest;
        if ((shifted<pawn_dir1>(k) | shifted<pawn_dir2>(k)) & pawns<!col>()) {
                if ((king<col>() & attack_map<!col>()) == false)
                        throw std::runtime_error("wrong check calc");
                return true;
        }

        if (king<col>() & attack_map<!col>())
                throw std::runtime_error("wrong check calc");

        return false;
#else
        constexpr bool is_white = col == Color::white;
        const OneSquare k = OneSquare_unsafe(king<col>());
        const Field friendly_occ = get_occupation<col>();
        const Field hostile_occ  = get_occupation<!col>();
        const Field all = friendly_occ | hostile_occ;

        const Field weak_straights = get_weakly_blocked_straights(k, all);
        if (weak_straights & (rooks<!col>() | queen<!col>()))
                return true;

        const Field weak_diags = get_weakly_blocked_diagonals(k, all);
        if (weak_diags & (bishops<!col>() | queen<!col>()))
                return true;

        if (get_horse_jumps(k) & horses<!col>())
                return true;

        constexpr Direction pawn_dir1 = is_white ? northEast : southEast;
        constexpr Direction pawn_dir2 = is_white ? northWest : southWest;
        if ((shifted<pawn_dir1>(k) | shifted<pawn_dir2>(k)) & pawns<!col>())
                return true;

        return false;
#endif
}


// if f is a bitmap field, all the bits of f
// will set the bits of the fields of color col to 0
// so if a white horse goes somewhere
// we can do set_all_0<black>(white_horse_to) to capture any black piece
// without checking which one
template<Color col>
constexpr
void PiecewiseBoard::set_all_0(const Field f)
{
        // the 1's in f determine which points are set to 0
        pawns<col>() &= ~f;
        king<col>() &= ~f;
        queen<col>() &= ~f;
        rooks<col>() &= ~f;
        horses<col>() &= ~f;
        bishops<col>() &= ~f;
}


template <Epiece piece>
constexpr
bool PiecewiseBoard::is_on(uint8_t rank, uint8_t file) const
{
        return board[piece] & OneSquare(rank, file);
}



constexpr
Position::Position(const Position &other)
        : PiecewiseBoard{other},
          meta(other.meta)
{
        copy_board(other);
}

constexpr void
Position::copy_board(const Position &other) noexcept
{
        std::copy_n(other.board, 12, board);
}

constexpr
Position &Position::operator=(const Position &other)
{
        copy_board(other);
        meta = other.meta;
        return *this;
}

/*
inline EngineBoardGame &EngineBoardGame::operator=(const EngineBoardGame &other)
{
        boards.clear();
        for (const auto &b : other.boards) {
                boards.emplace_back(b);
        }
        return *this;
}
*/


/*
// returns const & to the top board
[[nodiscard]]
inline const Position &EngineBoardGame::current() const noexcept
{
        return boards.back();
}
*/

constexpr MetaData start_meta = []() -> MetaData {
        MetaData meta;
        meta.passive_move_counter = 0;
        meta.pawn_2_fwd_file = 8;
        /*
        meta.black_king_castle = true;
        meta.white_king_castle = true;
        meta.black_queen_castle = true;
        meta.white_queen_castle = true;
        */
        meta.castle_rights = 0xF;
        meta.active = Color::white;
        return meta;
}();


constexpr Position start_position = []() constexpr -> Position {
        Position epwb;
        epwb.to_start();
        epwb.meta = start_meta;
        return epwb;
}();


constexpr Position empty_position = []() constexpr -> Position {
        Position epwb;
        std::for_each_n(epwb.board, 12, [](Field &f) {f = 0;});
        epwb.meta = start_meta; // whatever
        return epwb;
}();


/*
inline
EngineBoardGame::EngineBoardGame()
{
        Position start;
        start.to_start();
        boards.emplace_back(start);
}


// add a board to the game
// no checking is done
inline void EngineBoardGame::add_board(const Position &new_board)
{
        boards.emplace_back(new_board);
}

// takes back a move
inline void EngineBoardGame::take_back()
{
        if (boards.size() > 1) {
                boards.pop_back();
        }
}

// returns the color that may make a move
[[nodiscard]]
inline Color EngineBoardGame::turn() const
{
        return boards.back().meta.active_color;
}

[[nodiscard]]
inline GameState EngineBoardGame::get_state() const
{
        GameState state;
        const Color to_move = turn();

        // todo all

        return state;
}

// to starting game
constexpr
void EngineBoardGame::reset()
{
        boards.resize(1);
}
*/

constexpr
auto operator== (Move l, Move r)
{
        // both Specials must be the same
        Move::Special spec = l.get_special();
        if (r.get_special() != spec)
                return false;

        // in case of castling, only the castle type must be the same
        if (spec == Move::castle)
                return l.get_castle_type() == r.get_castle_type();

        // in case of promotion, the promoted piece must be the same as well
        if (spec == Move::Special::promotion && l.get_promotion() != r.get_promotion())
                return false;

        // everything must be equal here anyway
        return l.data == r.data;
}

// returns field with all places the horse can jump to

extern const std::array<Field, 64> horse_jumps_table;

inline
Field get_horse_jumps(OneSquare sq)
{
        if constexpr (horses_ct == CalculationType::lookup_table) {
                const int shift = square_to_shift(sq);
                return horse_jumps_table[shift];
        } else {
                Field ret = 0;
                ret |= sq << 15 & msk::RIGHT;   // nnw
                ret |= sq << 17 & msk::LEFT;    // nne
                ret |= sq << 10 & msk::LEFT2;   // nee
                ret |= sq << 6  & msk::RIGHT2;  // nww
                ret |= sq >> 15 & msk::LEFT;    // sse
                ret |= sq >> 17 & msk::RIGHT;   // ssw
                ret |= sq >> 10 & msk::RIGHT2;  // sww
                ret |= sq >> 6  & msk::LEFT2;   // see
                return ret;
        }
}



extern const std::array<Field, 64> king_area_lookup_table;

inline
Field get_king_area(OneSquare sq)
{
        if constexpr (kings_ct == CalculationType::lookup_table) {
                const int shift = square_to_shift(sq);
                return king_area_lookup_table[shift];
        } else {
                // the surrounding points are projected onto the "grid"

                Field area = sq;

                area |= shifted<north>(area);
                area |= shifted<south>(area);

                //                      0 1 0
                // area looks like      0 1 0   (unless edge)
                //                      0 1 0


                area |= shifted<east>(area);
                area |= shifted<west>(area);
                return area;
        }
}


#endif //BOT_ENGINE_BOARD_H