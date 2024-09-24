//
// Created by Hugo Bogaart on 18/07/2024.
//

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "position.h"
// #include <vector>
#include <array>

#include "zobrist-hash.h"
#include <numeric>

// try to make the move if legal
template <Color col>
constexpr
auto maybe_make_move (Move cpm, const Position &board) -> std::optional<Position>;


constexpr
auto maybe_make_move (Move cpm, const Position &board) -> std::optional<Position>
{
        return board.meta.active == Color::white ? maybe_make_move<Color::white>(cpm, board) : maybe_make_move<Color::black>(cpm, board);
}

// these functions are unsafe
// produces total nonsense if used for invalid input
template <Color col>
[[deprecated]]
inline
auto make_move_unsafe(Move cpm, Position &board) -> void;

// this overload
template <Color col>
inline
auto make_move_unsafe(Move cpm, PositionHashPair &pos_hash) -> void;

constexpr size_t maxMoves = 256;

struct MoveList {

        // iterator stuff
        typedef Move value_type;
        typedef Move *pointer;
        typedef Move &reference;
        typedef const Move *const_pointer;
        typedef const Move &const_reference;
        typedef Move *iterator;


        [[nodiscard]] constexpr
        auto begin () -> iterator {return list.begin();}
        [[nodiscard]] constexpr
        auto begin () const -> const_pointer {return list.cbegin();}
        [[nodiscard]] constexpr
        auto end () -> iterator {return end_p;}
        [[nodiscard]] constexpr
        auto end () const -> const_pointer {return end_p;}

        [[nodiscard]] constexpr
        auto operator[] (size_t i) const -> const_reference {return list[i];}
        [[nodiscard]] constexpr
        auto operator[] (size_t i) -> reference {return list[i];}

        constexpr
        auto push_back (Move mv) -> void {*end_p++ = mv;}

        template <typename... Args>
        constexpr
        auto emplace_back (Args&&... args) -> void {push_back(Move(std::forward<Args>(args)...));}

        [[nodiscard]] constexpr
        auto back () -> reference {return end_p[-1];}

        [[nodiscard]] constexpr
        auto size () const -> size_t {return end_p - list.data();}

        [[nodiscard]] constexpr
        auto empty () const -> bool {return end_p == list.data();}

        constexpr
        auto clear () -> void {end_p = begin();}

        MoveList()
                : list{},
                  end_p(list.data())
        { }

        std::array<Move, maxMoves> list;
        Move *end_p;
};


template <Color col>
inline
auto generate_moves (const Position &pos, MoveList &move_list) -> void;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
/// TEMPLATE DEFINITIONS
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

template <Color col>
constexpr
auto maybe_make_move(Move cpm, const Position &board) -> std::optional<Position>
{
        MoveList allowed_moves;
        generate_moves<col>(board, allowed_moves);

        const bool move_is_allowed = std::ranges::any_of(allowed_moves, [&](Move mv) -> bool {
                return cpm == mv;
        });

        if (!move_is_allowed)
                return std::nullopt;

        Position next_board = board;
        make_move_unsafe<col>(cpm, next_board);
        return next_board;
}

template <Color col>
inline
auto make_move_unsafe(Move cpm, Position &board) -> void
{
        const OneSquare from = cpm.from_square();
        const OneSquare to   = cpm.to_square();
        const uint8_t from_file = cpm.from_file();
        const uint8_t from_rank = cpm.from_rank();
        const uint8_t to_file = cpm.to_file();
        const uint8_t to_rank = cpm.to_rank();

        const Field from_and_to = from | to;

        // const Field all_this   = board.get_occupation<col>();
        const Field all_other = board.get_occupation<!col>();
        // const Field all       = all_col | all_other;


        // metadata:
        // * in all control paths we must set the active_color color to the other one
        // * in all control paths except pawn 2 forward we must set pawn2fwd to 8
        // * if the king moves we must disallow all castles that color
        // * if a rook moves from its home square we must disallow that side castle
        // * any move must either increment or reset the passive move counter


        // a normal move consists of the following:
        // - original piece is removed
        // - this piece is then placed at the to file
        // - any piece there is captured

        // castling and en passant is taken care of in a special case

        auto &meta = board.meta;
        meta.active = !col;

        switch (cpm.get_special()) {
        case Move::castle:
                // there can never be any capture with castling (we don't check)
                if (cpm.get_castle_type() == Move::CastleType::queenside) {
                        constexpr Field rook_from = white_black<col>(white_queen_rook, black_queen_rook);
                        constexpr Field rook_to   = white_black<col>(white_queen_rook_to, black_queen_rook_to);
                        constexpr Field rook_from_and_to = rook_from | rook_to;
                        constexpr Field king_from = white_black<col>(white_king_start, black_king_start);
                        constexpr Field king_to   = white_black<col>(white_king_queenside_to, black_king_queenside_to);
                        board.rooks<col>() ^= rook_from_and_to; // move rook
                        board.king<col>()  ^= king_from;
                        board.king<col>()  ^= king_to;

                        meta.set_pawn_2fwd(8);
                        meta.disallow_queen_castle<col>();
                        meta.disallow_king_castle<col>();
                        meta.inc_passive_move_counter();
                } else {
                        // kingside
                        constexpr Field rook_from = white_black<col>(white_king_rook, black_king_rook);
                        constexpr Field rook_to   = white_black<col>(white_king_rook_to, black_king_rook_to);
                        constexpr Field rook_from_and_to = rook_from | rook_to;
                        constexpr Field king_from = white_black<col>(white_king_start, black_king_start);
                        constexpr Field king_to   = white_black<col>(white_king_kingside_to, black_king_kingside_to);
                        board.rooks<col>() ^= rook_from_and_to;
                        board.king<col>()  ^= king_from;
                        board.king<col>()  ^= king_to;

                        meta.set_pawn_2fwd(8);
                        meta.disallow_king_castle<col>();
                        meta.disallow_queen_castle<col>();
                        meta.inc_passive_move_counter();
                }
                return;
        case Move::en_passant:
                {
                        constexpr Direction one_back  = white_black<col>(south, north);
                        board.pawns<col>() ^= from_and_to;      // move pawn
                        board.pawns<!col>() ^= shifted<one_back>(to); // capture the en-passanted pawn

                        meta.reset_passive_move_counter();
                        meta.set_pawn_2fwd(8);
                        return;
                }
        case Move::Special::promotion:
                // a pawn move with promotion
                // there could be a capture
                {
                        board.pawns<col>() ^= from;
                        switch (cpm.get_promotion()) {
                        case Move::queen_promo:
                                board.queen<col>() ^= to;
                                break;
                        case Move::rook_promo:
                                board.rooks<col>() ^= to;
                                break;
                        case Move::bishop_promo:
                                board.bishops<col>() ^= to;
                                break;
                        case Move::horse_promo:
                                board.horses<col>() ^= to;
                                break;
                        }
                        if (from_file != to_file)
                                board.set_all_0<!col>(to);

                        meta.reset_passive_move_counter();
                        meta.set_pawn_2fwd(8);
                        return;
                }

        default:
                break;
        }

        if (board.pawns<col>() & from) {
                // en passant and promotion have been handled already

                // in any case
                meta.reset_passive_move_counter();

                if (from_file == to_file) {
                        // straight
                        // one or two or promotion

                        constexpr Direction one_ahead = white_black<col>(north, south);

                        if (shifted<one_ahead>(from) == to) {
                                // one tile
                                board.pawns<col>() ^= from_and_to;
                                meta.set_pawn_2fwd(8);
                                return;
                        } else {
                                // two tiles
                                board.pawns<col>() ^= from_and_to;
                                meta.set_pawn_2fwd(square_file(from));
                                return;
                        }

                } else {
                        // diagonal
                        // normal capture

                        board.pawns<col>() ^= from_and_to;
                        board.set_all_0<!col>(to); // capture
                        meta.set_pawn_2fwd(8);
                        return;
                        // en passant and promotion has been taken care of
                }
        }

        // any normal move with a piece (not pawn)
        // no castling

        if (from & board.horses<col>()) {
                // board.horses<col>() ^= from;
                // board.horses<col>() ^= to;
                board.horses<col>() ^= from_and_to;
        } else if (from & board.bishops<col>()) {
                board.bishops<col>() ^= from_and_to;
        } else if (from & board.rooks<col>()) {
                board.rooks<col>() ^= from_and_to;

                constexpr Field queenside_rook = white_black<col>(white_queen_rook, black_queen_rook);
                constexpr Field kingside_rook = white_black<col>(white_king_rook, black_king_rook);

                if (from & queenside_rook) {
                        meta.disallow_queen_castle<col>();
                } else if (from & kingside_rook) {
                        meta.disallow_king_castle<col>();
                }

        } else if (from & board.king<col>()) {
                board.king<col>() ^= from_and_to;
                meta.disallow_queen_castle<col>();
                meta.disallow_king_castle<col>();
        } else /* no alternative but queen */ {
                board.queen<col>() ^= from_and_to;
        }

        // capture with any piece
        if (to & all_other) {
                board.set_all_0<!col>(to);
                meta.reset_passive_move_counter();
        } else {
                meta.inc_passive_move_counter();
        }

        meta.set_pawn_2fwd(8);
}

template <Color col>
inline
auto make_move_unsafe(Move cpm, PositionHashPair &pos_hash) -> void
{
        using namespace HashConstants;

        Position &board = pos_hash.pos;
        uint64_t &hash  = pos_hash.hash;

        constexpr Epiece colking = white_black<col>(white_king, black_king);
        constexpr Epiece colqueen = white_black<col>(white_queen, black_queen);
        constexpr Epiece colrook = white_black<col>(white_rooks, black_rooks);
        constexpr Epiece colhorse = white_black<col>(white_horses, black_horses);
        constexpr Epiece colbishop = white_black<col>(white_bishops, black_bishops);
        constexpr Epiece colpawn = white_black<col>(white_pawns, black_pawns);

        constexpr Epiece othercolpawn = white_black<col>(black_pawns, white_pawns);


        const OneSquare from = cpm.from_square();
        const OneSquare to   = cpm.to_square();
        const uint8_t from_file = cpm.from_file();
        const uint8_t from_rank = cpm.from_rank();
        const uint8_t to_file = cpm.to_file();
        const uint8_t to_rank = cpm.to_rank();

        const Field from_and_to = from | to;

        // const Field all_this   = board.get_occupation<col>();
        // const Field all_other = board.get_occupation<!col>();
        // const Field all       = all_col | all_other;

        auto check_for_capture = [&](Field f) -> std::optional<Epiece> {
                if (board.pawns<!col>() & f)
                        return white_black<col>(black_pawns, white_pawns);
                if (board.rooks<!col>() & f)
                        return white_black<col>(black_rooks, white_rooks);
                if (board.bishops<!col>() & f)
                        return white_black<col>(black_bishops, white_bishops);
                if (board.horses<!col>() & f)
                        return white_black<col>(black_horses, white_horses);
                if (board.queen<!col>() & f)
                        return white_black<col>(black_queen, white_queen);
                // king is not captured anyway
                // todo
                return std::nullopt;
        };

        // metadata:
        // * in all control paths we must set the active_color color to the other one
        // * in all control paths except pawn 2 forward we must set pawn2fwd to 8
        // * if the king moves we must disallow all castles that color
        // * if a rook moves from its home square we must disallow that side castle
        // * any move must either increment or reset the passive move counter


        // a normal move consists of the following:
        // - original piece is removed
        // - this piece is then placed at the to file
        // - any piece there is captured

        // castling and en passant is taken care of in a special case

        // we must also update the hash code
        // each time we move a piece we must update the removal, and placement with piece_square_hash
        // en passant-file, castle rights and color to play are relavant as well

        auto &meta = board.meta;
        meta.active = !col;
        hash ^= black_move_hash; // always toggle anyway


        switch (cpm.get_special()) {
        case Move::castle:
                // there can never be any capture with castling (we don't check)
                if (cpm.get_castle_type() == Move::CastleType::queenside) {
                        constexpr Field rook_from = white_black<col>(white_queen_rook, black_queen_rook);
                        constexpr Field rook_to   = white_black<col>(white_queen_rook_to, black_queen_rook_to);
                        constexpr Field rook_from_and_to = rook_from | rook_to;
                        constexpr Field king_from = white_black<col>(white_king_start, black_king_start);
                        constexpr Field king_to   = white_black<col>(white_king_queenside_to, black_king_queenside_to);

                        // move pieces
                        board.rooks<col>() ^= rook_from_and_to;
                        board.king<col>()  ^= king_from;
                        board.king<col>()  ^= king_to;

                        // get some hash constants
                        constexpr int rook_from_shift = trailing_0_count(rook_from);
                        constexpr int rook_to_shift = trailing_0_count(rook_to);
                        constexpr int king_from_shift = trailing_0_count(king_from);
                        constexpr int king_to_shift = trailing_0_count(king_to);

                        constexpr uint64_t piece_change_hash = piece_square_hash(colrook, rook_from_shift)
                                ^ piece_square_hash(colrook, rook_to_shift)
                                ^ piece_square_hash(colking, king_from_shift)
                                ^ piece_square_hash(colking, king_to_shift);

                        // update hash value
                        hash ^= piece_change_hash;

                } else {
                        // kingside
                        constexpr Field rook_from = white_black<col>(white_king_rook, black_king_rook);
                        constexpr Field rook_to   = white_black<col>(white_king_rook_to, black_king_rook_to);
                        constexpr Field rook_from_and_to = rook_from | rook_to;
                        constexpr Field king_from = white_black<col>(white_king_start, black_king_start);
                        constexpr Field king_to   = white_black<col>(white_king_kingside_to, black_king_kingside_to);
                        board.rooks<col>() ^= rook_from_and_to;
                        board.king<col>()  ^= king_from;
                        board.king<col>()  ^= king_to;

                        // get some hash constants
                        // square_to_shift === trailing_0_count
                        constexpr int rook_from_shift = trailing_0_count(rook_from);
                        constexpr int rook_to_shift = trailing_0_count(rook_to);
                        constexpr int king_from_shift = trailing_0_count(king_from);
                        constexpr int king_to_shift = trailing_0_count(king_to);

                        constexpr uint64_t piece_change_hash = piece_square_hash(colrook, rook_from_shift)
                                ^ piece_square_hash(colrook, rook_to_shift)
                                ^ piece_square_hash(colking, king_from_shift)
                                ^ piece_square_hash(colking, king_to_shift);

                        // update hash value
                        hash ^= piece_change_hash;

                }

                // unset any previous hash for this
                hash ^= en_passant_hash(meta.pawn2fwd_file());
                meta.set_pawn_2fwd(8);

                // unset castle rights hash
                hash ^= castling_right_hash(meta.castle_rights);

                // and update
                meta.disallow_queen_castle<col>();
                meta.disallow_king_castle<col>();
                hash ^= castling_right_hash(meta.castle_rights);

                meta.inc_passive_move_counter();
                return;

        case Move::en_passant:
                {
                        constexpr Direction one_back = white_black<col>(south, north);
                        const Field enemy_pawn_sq = shifted<one_back>(to);
                        board.pawns<col>() ^= from_and_to;      // move pawn
                        board.pawns<!col>() ^= enemy_pawn_sq; // capture the en-passanted pawn

                        // move hash

                        hash ^= piece_square_hash(colpawn, square_to_shift(from));
                        hash ^= piece_square_hash(colpawn, square_to_shift(to));

                        // capture hash
                        hash ^= piece_square_hash(othercolpawn, trailing_0_count(enemy_pawn_sq));

                        meta.reset_passive_move_counter();

                        // pawn2fwd hash
                        hash ^= en_passant_hash(meta.pawn2fwd_file());
                        meta.set_pawn_2fwd(8);
                        return;
                }
        case Move::Special::promotion:
                // a pawn move with promotion
                // there could be a capture
                {

                        board.pawns<col>() ^= from;
                        hash ^= piece_square_hash(colpawn, square_to_shift(from));

                        switch (cpm.get_promotion()) {
                        case Move::queen_promo:
                                board.queen<col>() ^= to;
                                hash ^= piece_square_hash(colqueen, square_to_shift(to));
                                break;
                        case Move::rook_promo:
                                board.rooks<col>() ^= to;
                                hash ^= piece_square_hash(colrook, square_to_shift(to));
                                break;
                        case Move::bishop_promo:
                                board.bishops<col>() ^= to;
                                hash ^= piece_square_hash(colbishop, square_to_shift(to));
                                break;
                        case Move::horse_promo:
                                board.horses<col>() ^= to;
                                hash ^= piece_square_hash(colhorse, square_to_shift(to));
                                break;
                        }
                        // maybe capture
                        // if (from_file != to_file)
                        //        board.set_all_0<!col>(to);

                        std::optional<Epiece> pc = check_for_capture(to);
                        if (pc) {
                                board.piece_field(*pc) ^= to; // capture
                                hash ^= piece_square_hash(*pc, square_to_shift(to));
                        }

                        meta.reset_passive_move_counter();

                        hash ^= en_passant_hash(meta.pawn2fwd_file());
                        meta.set_pawn_2fwd(8);
                        return;
                }

        default:
                break;
        }

        if (board.pawns<col>() & from) {
                // en passant and promotion have been handled already

                // in any case
                meta.reset_passive_move_counter();

                if (from_file == to_file) {
                        // straight
                        // one or two or promotion

                        constexpr Direction one_ahead = white_black<col>(north, south);

                        if (shifted<one_ahead>(from) == to) {
                                // one tile
                                board.pawns<col>() ^= from_and_to;
                                hash ^= piece_square_hash(colpawn, square_to_shift(from));
                                hash ^= piece_square_hash(colpawn, square_to_shift(to));

                                hash ^= en_passant_hash(meta.pawn2fwd_file());
                                meta.set_pawn_2fwd(8);
                                return;
                        } else {
                                // two tiles
                                board.pawns<col>() ^= from_and_to;
                                hash ^= piece_square_hash(colpawn, square_to_shift(from));
                                hash ^= piece_square_hash(colpawn, square_to_shift(to));

                                hash ^= en_passant_hash(meta.pawn2fwd_file());
                                meta.set_pawn_2fwd(square_file(from));
                                hash ^= en_passant_hash(meta.pawn2fwd_file());
                                return;
                        }

                } else {
                        // diagonal
                        // normal capture

                        // is always a capture, so can just * it todo
                        Epiece pc = *check_for_capture(to); // more expensive check than the files

                        board.pawns<col>() ^= from_and_to;
                        hash ^= piece_square_hash(colpawn, square_to_shift(from));
                        hash ^= piece_square_hash(colpawn, square_to_shift(to));

                        board.piece_field(pc) ^= to;
                        hash ^= piece_square_hash(pc, square_to_shift(to));

                        hash ^= en_passant_hash(meta.pawn2fwd_file());
                        meta.set_pawn_2fwd(8);
                        return;
                        // en passant and promotion has been taken care of
                }
        }

        // any normal move with a piece (not pawn)
        // no castling

        if (from & board.horses<col>()) {
                // board.horses<col>() ^= from;
                // board.horses<col>() ^= to;
                board.horses<col>() ^= from_and_to;
                hash ^= piece_square_hash(colhorse, square_to_shift(from));
                hash ^= piece_square_hash(colhorse, square_to_shift(to));
        } else if (from & board.bishops<col>()) {
                board.bishops<col>() ^= from_and_to;
                hash ^= piece_square_hash(colbishop, square_to_shift(from));
                hash ^= piece_square_hash(colbishop, square_to_shift(to));
        } else if (from & board.rooks<col>()) {
                board.rooks<col>() ^= from_and_to;
                hash ^= piece_square_hash(colrook, square_to_shift(from));
                hash ^= piece_square_hash(colrook, square_to_shift(to));

                constexpr Field queenside_rook = white_black<col>(white_queen_rook, black_queen_rook);
                constexpr Field kingside_rook = white_black<col>(white_king_rook, black_king_rook);

                const uint8_t start_castlerights = meta.castle_rights;
                if (from & queenside_rook) {
                        meta.disallow_queen_castle<col>();
                } else if (from & kingside_rook) {
                        meta.disallow_king_castle<col>();
                }
                if (start_castlerights != meta.castle_rights) {
                        hash ^= castling_right_hash(start_castlerights);
                        hash ^= castling_right_hash(meta.castle_rights);
                }

        } else if (from & board.king<col>()) {
                board.king<col>() ^= from_and_to;
                hash ^= piece_square_hash(colking, square_to_shift(from));
                hash ^= piece_square_hash(colking, square_to_shift(to));

                const uint8_t start_castlerights = meta.castle_rights;
                meta.disallow_queen_castle<col>();
                meta.disallow_king_castle<col>();
                if (start_castlerights != meta.castle_rights) {
                        hash ^= castling_right_hash(start_castlerights);
                        hash ^= castling_right_hash(meta.castle_rights);
                }
        } else /* no alternative but queen */ {
                board.queen<col>() ^= from_and_to;
                hash ^= piece_square_hash(colqueen, square_to_shift(from));
                hash ^= piece_square_hash(colqueen, square_to_shift(to));
        }

        // capture
        std::optional<Epiece> captured = check_for_capture(to);
        if (captured) {
                // no en passant, so always capture on the square we go to
                board.piece_field(*captured) ^= to;
                hash ^= piece_square_hash(*captured, square_to_shift(to));

                meta.reset_passive_move_counter();
        } else {
                meta.inc_passive_move_counter();
        }

        hash ^= en_passant_hash(meta.pawn2fwd_file());
        meta.set_pawn_2fwd(8);
}

/*
template <Color col>
inline
auto generate_moves (const Position &pos, MoveList &move_list) -> void
{
        // TODO don't check for legality afterwards but mark all attacked squares and pinned pieces

        constexpr Color other_col = !col;
        const Field all_friendly = pos.get_occupation<col>();
        const Field all_hostile = pos.get_occupation<other_col>();
        const Field total = all_friendly | all_hostile;        // has all non-empty squares

        // reference to primitive board
        const PiecewiseBoard &pos_board = pos;

        // on this board we will see if there is no check, so that the move is allowed
        // Position proposal;


        // repetition draw rule
        // if non-passive the counter is reset once the board is in the list

        // todo this is disgusting
        OneSquare move_to_os;
        Field &move_to = move_to_os.fd;

        Field all_available;    // stores all squares a piece can go to

        Field *to_piece_field;
        // points to the field of a specific piece
        // ideally this would be a reference, but that can't rebind

        // takes the direction relative to col, and returns the direction relative to white
        // so the direction that the pawn goes forward is always abs_dir(north)
        auto abs_dir = [](Direction dir) -> Direction {
                return white_black<col>(dir, flip_dir(dir));
        };

        // simple loop will visit all pieces, and for each piece we add all moves that piece can make
        for (OneSquare move_from : all_squares) {
                PiecewiseBoard prop = pos_board;

                if (!(move_from & all_friendly)) {
                        // no piece of this color present on this square
                        // might as well skip
                        continue;
                }

                // pawns
                // there are different ways to move
                // one forward, maybe promotion
                // two forward
                // two diagonals, maybe en passent or maybe promoting
                else if (pos.pawns<col>() & move_from) {
                        // pawn


                        //  for promotion, we only have to check if one of the promotions is allowed, then they are all allowed
                        // they can happen together with any other move, except 2 fwd or en passant
                        // variations, Q, R, H, B

                        constexpr auto promotion_rank = white_black<col>(msk::rank[7], msk::rank[0]);
                        constexpr auto pawn_start_rank = white_black<col>(msk::rank[1], msk::rank[6]);
                        constexpr auto en_passant_rank = white_black<col>(msk::rank[5], msk::rank[2]);


                        // try one tile ahead first
                        // to allow one tile it must be empty
                        move_to = shifted<abs_dir(north)>(move_from);

                        if (!(move_to & total)) { // case the tile is available

                                prop.pawns<col>() ^= move_from;    // remove the initial pawn
                                prop.pawns<col>() ^= move_to;      // and add it in its new place
                                if (!prop.in_check<col>()) {
                                        // move allowed

                                        if (move_to & promotion_rank) {
                                                // any promotion is possible
                                                move_list.emplace_back(move_from, move_to_os, Move::queen_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::rook_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::horse_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::bishop_promo);
                                        } else {
                                                move_list.emplace_back(move_from, move_to_os);
                                        }
                                }
                                // and restore
                                prop.pawns<col>() = pos.pawns<col>();

                                // also try 2 tiles ahead
                                // the pawn has to be on the second or sixth rank, depending on color
                                if (move_from & pawn_start_rank) {
                                        shift<abs_dir(north)>(move_to);
                                        if (!(move_to & total)) {
                                                prop.pawns<col>() ^= move_from;    // remove the initial pawn
                                                prop.pawns<col>() ^= move_to;       // and add it in
                                                if (!prop.in_check<col>()) {
                                                        move_list.emplace_back(move_from, move_to_os);
                                                }
                                                prop.pawns<col>() = pos.pawns<col>();
                                        }
                                }
                        }

                        // done going forward
                        // attack up right

                        move_to = shifted<abs_dir(northEast)>(move_from);
                        if (move_to & all_hostile) {
                                // the move_to square exists and
                                // there is a piece there to capture

                                prop.pawns<col>() ^= move_from;    // remove the initial pawn
                                prop.pawns<col>() ^= move_to;       // and add it in
                                prop.set_all_0<other_col>(move_to);    // capture anything
                                if (!prop.in_check<col>()) {
                                        // move allowed

                                        // look for promotion
                                        if (move_to & promotion_rank) {
                                                // any promotion is possible
                                                move_list.emplace_back(move_from, move_to_os, Move::queen_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::rook_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::horse_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::bishop_promo);
                                        } else {
                                                move_list.emplace_back(move_from, move_to_os);
                                        }
                                }
                                // restore
                                prop = pos_board;
                        } else if (move_to & en_passant_rank & msk::file[pos.meta.pawn2fwd_file()]) {
                                // no piece here, but en passant

                                // this condition tests if the *to* field is both in the appropriate rank and file
                                // en passant can be done when to is on rank 6/3 (5/2 with 0 index) and to the appropriate file
                                // last move the pawn must have done 2 fwd on the file this pawn is going to
                                // we don't have to look for pawn promotion anyway

                                prop.pawns<col>() ^= move_from;    // remove the initial pawn
                                prop.pawns<col>() ^= move_to;       // and add it in
                                prop.pawns<other_col>() ^= shifted<abs_dir(south)>(move_to);       // capture one down from the pawn
                                if (!prop.in_check<col>()) {
                                        // move allowed
                                        move_list.emplace_back(move_from, move_to_os, Move::EnPassant);
                                }
                                // only pawn fields have to be restored
                                prop.pawns<other_col>() = pos.pawns<other_col>();
                                prop.pawns<col>() = pos.pawns<col>();
                        }

                        // and totally identical for the other diagonal

                        move_to = shifted<abs_dir(northWest)>(move_from);
                        if (move_to & all_hostile) {
                                // the move_to square exists and
                                // there is a piece there to capture

                                // normal capture
                                prop.pawns<col>() ^= move_from;    // remove the initial pawn
                                prop.pawns<col>() ^= move_to;       // and add it in
                                prop.set_all_0<other_col>(move_to);    // capture anything
                                if (!prop.in_check<col>()) {
                                        // move allowed

                                        // look for promotion
                                        if (move_to & promotion_rank) {
                                                // any promotion is possible
                                                move_list.emplace_back(move_from, move_to_os, Move::queen_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::rook_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::horse_promo);
                                                move_list.emplace_back(move_from, move_to_os, Move::bishop_promo);
                                        } else {
                                                move_list.emplace_back(move_from, move_to_os);
                                        }
                                }
                                // restore
                                prop = pos_board;

                        } else if (move_to & en_passant_rank & msk::file[pos.meta.pawn2fwd_file()]) {

                                prop.pawns<col>() ^= move_from;    // remove the initial pawn
                                prop.pawns<col>() ^= move_to;       // and add it in
                                prop.pawns<other_col>() ^= shifted<abs_dir(south)>(move_to);       // capture one down from the pawn
                                if (!prop.in_check<col>()) {
                                        // move allowed

                                        move_list.emplace_back(move_from, move_to_os, Move::EnPassant);
                                }
                                // only pawn fields have to be restored
                                prop.pawns<other_col>() = pos.pawns<other_col>();
                                prop.pawns<col>() = pos.pawns<col>();
                        }
                        continue;
                }
                // continue

                // horse -> just check all 8
                else if (pos.horses<col>() & move_from) {
                        // we just try all the horse moves and see if we are not in check after that

                        move_to = shifted_horse<NNE>(move_from);

                        // the to field must be non-zero and tile available
                        // then we make the move on proposal and see if it is allowed
                        auto add_horse_move = [&]() {
                                // the to field must be non-zero and tile available
                                if (move_to & ~all_friendly) {
                                        prop.horses<col>() ^= move_from;        // remove horse from position
                                        prop.horses<col>() ^= move_to;           // place horse at new point
                                        prop.set_all_0<other_col>(move_to);   // capture any piece that might be present
                                        if (!prop.in_check<col>()) {
                                                move_list.emplace_back(move_from, move_to_os);
                                        }
                                        // and restore the entire board
                                        prop = pos_board;
                                }
                        };

                        add_horse_move();
                        move_to = shifted_horse<NEE>(move_from);
                        add_horse_move();
                        move_to = shifted_horse<SEE>(move_from);
                        add_horse_move();
                        move_to = shifted_horse<SSE>(move_from);
                        add_horse_move();
                        move_to = shifted_horse<SSW>(move_from);
                        add_horse_move();
                        move_to = shifted_horse<SWW>(move_from);
                        add_horse_move();
                        move_to = shifted_horse<NWW>(move_from);
                        add_horse_move();
                        move_to = shifted_horse<NNW>(move_from);
                        add_horse_move();

                        continue;
                }
                // continue

                // these only find the tiles the pieces can move to, not if they are allowed or not
                // to_piece_field will point to the piece that is being considered
                // all_available will be the field that has all the squares the piece can move to

                // rooks -> only straights
                else if (move_from & pos.rooks<col>()) {
                        all_available = get_weakly_blocked_straights(move_from, total);
                        all_available &= ~all_friendly;
                        to_piece_field = &prop.rooks<col>();
                }

                // bishop -> only diagonals
                else if (move_from & pos.bishops<col>()) {

                        all_available = get_weakly_blocked_diagonals(move_from, total);
                        all_available &= ~all_friendly;
                        to_piece_field = &prop.bishops<col>();
                }

                // queen -> both straights and diagonals
                else if (move_from & pos.queen<col>()) {
                        all_available  = get_weakly_blocked_straights(move_from, total);
                        all_available |= get_weakly_blocked_diagonals(move_from, total);
                        all_available &= ~all_friendly;
                        to_piece_field = &prop.queen<col>();
                }

                // king -> normal moves and castling
                else { // if (move_from & pos.king<col>()) {
                        // the normal moves
                        all_available  = get_king_area(move_from) & ~all_friendly;
                        to_piece_field = &prop.king<col>();

                        // now do castling, the normal king moves are handled separately
                        // using all_available

                        // some named constants regarding some relevant area's on the board
                        constexpr auto queenside_rook = white_black<col>(white_queen_rook, black_queen_rook);
                        constexpr auto kingside_rook = white_black<col>(white_king_rook, black_king_rook);
                        constexpr auto king_start = white_black<col>(white_king_start, black_king_start);

                        // squares that must be unobstructed
                        constexpr auto queenside_freezone = white_black<col>(white_castle_queen_freezone, black_castle_queen_freezone);
                        constexpr auto kingside_freezone = white_black<col>(white_castle_king_freezone, black_castle_king_freezone);

                        // squares that must not be under attack
                        constexpr auto queenside_safezone = white_black<col>(white_castle_queen_safezone, black_castle_queen_safezone);
                        constexpr auto kingside_safezone = white_black<col>(white_castle_king_safezone, black_castle_king_safezone);

                        // queen side

                        // we only calculate if needed
                        Field other_attack_map = 0;

                        bool queenside_allowed = pos.meta.get_queen_castle<col>() &&
                                                 pos.king<col>() == king_start &&
                                                 pos.rooks<col>() & queenside_rook && // excessive ?
                                                 (queenside_freezone & total) == 0;

                        if (queenside_allowed) {
                                // calculate attack map
                                other_attack_map = pos.attack_map<other_col>();
                                if ((other_attack_map & queenside_safezone) == 0) {
                                        // final blessing
                                        move_list.emplace_back(queen_castle_mv);
                                }
                        }

                        // now try kingside;

                        bool kingside_allowed = pos.meta.get_king_castle<col>() &&
                                                pos.king<col>() == king_start && // already done on queenside
                                                pos.rooks<col>() & kingside_rook && // excessive ?
                                                (kingside_freezone & total) == 0;

                        if (kingside_allowed) {
                                // calculate attack map if needed
                                if (other_attack_map == 0)
                                        other_attack_map = pos.attack_map<other_col>();

                                if ((other_attack_map & kingside_safezone) == 0) {
                                        move_list.emplace_back(king_castle_mv);
                                }
                        }
                }

                // for the last 4 we need to consider all the allowed squares the piece can go to
                // this in encoded in the all_available Field
                // to_piece_field points to the piece we are considering

                for (OneSquare to : all_squares) {
                        if (!(to & all_available)) {
                                // not an available square
                                continue;
                        }

                        *to_piece_field ^= move_from;       // pointer to the field within proposal of the piece we are moving
                        *to_piece_field ^= to;

                        prop.set_all_0<other_col>(to);
                        // for each of these moves we need to see if we are in check or not
                        if (!prop.in_check<col>())
                                move_list.emplace_back(move_from, to);

                        prop = pos_board;
                }
        }
}


template <Color col>
inline
auto generate_moves2 (const Position &pos, MoveList &move_list) -> void
{
        // not micro optimized

        constexpr bool is_white = col == Color::white;
        constexpr Color other_col = !col;
        const Field all_friendly = pos.get_occupation<col>();
        const Field all_hostile  = pos.get_occupation<other_col>();
        const Field total = all_friendly | all_hostile;        // has all non-empty squares

        // todo this is ugly as hell

        // const Field king_ = pos.king<col>();
        // const OneSquare &king = *reinterpret_cast<const OneSquare *>(&king_);
        const OneSquare king = OneSquare_unsafe(pos.king<col>());
        // const Field atm = pos.attack_map<other_col>();

        constexpr Field en_passant_rank  = msk::rank[is_white ? 4 : 3];
        constexpr Field back_rank        = msk::rank[is_white ? 7 : 0];
        constexpr Field second_back_rank = msk::rank[is_white ? 6 : 1];
        constexpr Field second_rank      = msk::rank[is_white ? 1 : 6];
        constexpr Direction ahead = is_white ? north : south;
        
        // the pinned array contains for each direction,
        // the squares between the king and an active attacker, if any
        // or 0 if there is no attacker from that direction
        // if we capture the attacking piece, we're good too

        std::array<Field, 8> pinned = {};

        // the pieces that are causing the associated pins
        std::array<Field, 8> pin_causers = {};

        // there is a very special case where there are two pawns next to each other
        // with a king and enemy rook on each side
        // one pawn captures the other en passant, and the rook sees the king
        // this is such a problematic case that we just do this
        // there can only be one allowed en passant anyway
        bool en_passant_pinned = false;

        // is 0 if there is none
        const Field two_moved_pawn = msk::file[pos.meta.pawn2fwd_file()] & en_passant_rank;

        // contains the squares that can capture a pawn en passant, if any
        const Field en_passant_squares = shifted<east>(two_moved_pawn) | shifted<west>(two_moved_pawn);

        // we find the pins first
        const Field straight_attackers = pos.rooks<other_col>() | pos.queen<other_col>();
        const Field diagonal_attackers = pos.bishops<other_col>() | pos.queen<other_col>();

        auto calculate_pin = [&]<Direction dir> () {
                const Field &danger = is_straight(dir) ? straight_attackers : diagonal_attackers;
                const Field ray = get_weakly_blocked_ray<dir>(king, danger);

                if (ray & danger) {
                        const Field in_between = ray & ~danger;
                        // there is only a pin with 1 friendly and 0 hostile pieces
                        // unless the rare case with the en passant rook thing
                        // an en passant capture can also expose the king via a diagonal

                        const int num_friendly_between = bit_count(all_friendly & in_between);
                        const int num_enemy_between = bit_count(all_hostile & in_between);
                        if (num_friendly_between == 1 && num_enemy_between == 0) {
                                pinned[dir] = in_between;
                                pin_causers[dir] = ray & danger;
                        } else if (is_diagonal(dir) && num_friendly_between == 0 && num_enemy_between == 1 && (in_between & two_moved_pawn)) {
                                // we can not capture en passant, because that would expose the king
                                en_passant_pinned = true;
                        } else if constexpr (dir == east || dir == west) {

                                const bool is_special_case = num_friendly_between == 1
                                            && num_enemy_between == 1
                                            && two_moved_pawn & in_between // indeed a pawn in between that can be taken en passant
                                            && en_passant_squares & pos.pawns<col>() & in_between // friendly pawn next to the enemy pawn
                                            ; // redundant && king & en_passant_rank; // and the king has to be there as well

                                if (is_special_case) {
                                        en_passant_pinned = true;
                                }
                        }

                }
        };

        // returns true if this move is ILLEGAL due to a pin
        // does not take into account the special en passant case
        // it does take into account the normal en passant case
        auto pin_prevents = [&](const OneSquare &from, const OneSquare &to) -> bool {
                // if "from" is pinned, it can only move within the ray
                for (const Direction dir : directions) {
                        const Field &pinned_areas = pinned[dir];
                        const Field &pin_causer = pin_causers[dir];

                        if (from & pinned_areas) {
                                // if "to" is within the ray, we return false, since we can move within the pinned area
                                // if we capture the pinner, this is fine too

                                return (to & (pinned_areas | pin_causer)) == 0ull;
                        }
                }
                return false;
        };

        // thanks, Bjarne!
        calculate_pin.template operator()<north>();
        calculate_pin.template operator()<northEast>();
        calculate_pin.template operator()<northWest>();
        calculate_pin.template operator()<south>();
        calculate_pin.template operator()<southEast>();
        calculate_pin.template operator()<southWest>();
        calculate_pin.template operator()<west>();
        calculate_pin.template operator()<east>();

        const Field active_horse_attackers = pos.horses<other_col>() & get_horse_jumps(king);
        const Field active_diagonal_attackers = diagonal_attackers & get_weakly_blocked_diagonals(king, total);
        const Field active_straight_attackers = straight_attackers & get_weakly_blocked_straights(king, total);
        const Field active_pawn_attackers = [&]() -> Field {
                if constexpr (is_white)
                        return pos.pawns<other_col>() & (shifted<northEast>(king) | shifted<northWest>(king));
                return pos.pawns<other_col>() & (shifted<southEast>(king) | shifted<southWest>(king));
        }();

        const Field all_active_attackers = active_horse_attackers | active_diagonal_attackers | active_straight_attackers | active_pawn_attackers;

        // king is in check
        // we must do something about it
        if (all_active_attackers) {

                // depending on what attacks us, we generate moves
                // if there is only one attacker, capturing with an unpinned piece is an option
                // as well as blocking

                // evasion is always an option, also with multiple attackers
                const int num_attackers = bit_count(all_active_attackers);

                // if only one piece attacks the king, capturing or blocking is an option,
                // provided we respect pins
                // otherwise, we only have evasions

                if (num_attackers == 1) {
                        const Field active_attacker_ = [&]() -> Field {
                                if (active_diagonal_attackers)
                                        return active_diagonal_attackers;
                                if (active_straight_attackers)
                                        return active_straight_attackers;
                                if (active_horse_attackers)
                                        return active_horse_attackers;
                                return active_pawn_attackers;
                        }();

                        // ugly as hell, yes. Too bad!
                        // const OneSquare &active_attacker = *reinterpret_cast<const OneSquare *>(&active_attacker_);
                        const OneSquare active_attacker = OneSquare_unsafe(active_attacker_);

                        // find pieces of ours that can capture the attacker
                        // no pinned pieces of course

                        // squares where we can place a piece of ours to block
                        Field block_area     = 0ull;

                        Field weak_straights = 0ull;
                        Field weak_diags     = 0ull;

                        // const Field weak_straights = get_weakly_blocked_straights(active_attacker, total);
                        // const Field weak_diags     = get_weakly_blocked_diagonals(active_attacker, total);

                        // todo is this less eficient than just blindly checking king & ray?
                        bool found_king = false;

                        // routine that looks at a ray, starting from the attacker in a direction
                        // and if the king is there, sets the block_area
                        auto handle_ray = [&]<Direction dir>() -> void {
                                const Field ray = get_weakly_blocked_ray<dir>(active_attacker, total);
                                if (!found_king && king & ray) {
                                        found_king = true;
                                        block_area = ray & ~king;
                                }

                                if constexpr (is_straight(dir)) {
                                        weak_straights |= ray;
                                } else {
                                        weak_diags |= ray;
                                }
                        };

                        handle_ray.template operator()<north>();
                        handle_ray.template operator()<east>();
                        handle_ray.template operator()<south>();
                        handle_ray.template operator()<west>();
                        handle_ray.template operator()<northEast>();
                        handle_ray.template operator()<southEast>();
                        handle_ray.template operator()<southWest>();
                        handle_ray.template operator()<northWest>();

                        // here, block_area, weak_straights, weak_diags have been set
                        // the attack is blockable <==> block_area != 0;
                        const Field horse_area     = get_horse_jumps(active_attacker);
                        const Field pawn_area      = is_white ? (shifted<southEast>(active_attacker) | shifted<southWest>(active_attacker))
                                                              : (shifted<northEast>(active_attacker) | shifted<northWest>(active_attacker));
                        const Field king_area      = get_king_area(active_attacker) ;

                        // if the attacking piece is a pawn that just moved 2 forward, there are some additional places
                        // that a pawn can stand on to capture this pawn

                        // we can only capture en passant if this indeed removes the check
                        const Field en_passant_area   = (active_attacker & two_moved_pawn) ? en_passant_squares : 0ull;

                        // these are exactly the candidates in position to capture the attacker
                        const Field candidate_rooks   = pos.rooks<col>()   & weak_straights;
                        const Field candidate_bishops = pos.bishops<col>() & weak_diags;
                        const Field candidate_queens  = pos.queen<col>()   & (weak_diags | weak_straights);
                        const Field candidate_horses  = pos.horses<col>()  & horse_area;
                        const Field candidate_pawns   = pos.pawns<col>()   & pawn_area;
                        const Field candidate_kings   = pos.king<col>()    & king_area;
                        // we'll treat en passant captures as special cases
                        const Field candidate_en_passant_pawns = pos.pawns<col>() & en_passant_area;

                        const Field all_candidates = candidate_rooks  | candidate_bishops | candidate_queens
                                                   | candidate_horses | candidate_pawns   | candidate_kings | candidate_en_passant_pawns;


                        // first we add the moves that capture the attacker
                        // king moves here are counted as EVASIONS, because that is easier
                        for (const OneSquare &from : all_squares) {
                                bool is_en_passant = false;

                                if ((from & all_candidates) == 0ull)
                                        continue;

                                if (from & king)
                                        continue;

                                // we set the en passant flag if applicable
                                // en passant captures are pinned slightly differently
                                if (from & candidate_en_passant_pawns) {
                                        // const Field to_= shifted<ahead>(active_attacker);
                                        // const OneSquare &to = *reinterpret_cast<const OneSquare *>(&to_);
                                        const OneSquare to = OneSquare_unsafe(shifted<ahead>(active_attacker));

                                        if (en_passant_pinned || pin_prevents(from, to)) {
                                                continue;
                                        }
                                        is_en_passant = true;
                                        // the en passant pawn can be pinned as well
                                } else if (pin_prevents(from, active_attacker)) {
                                        // we still have to consider normal pins
                                        continue;
                                }

                                // now we add the move, since it is clearly allowed
                                if (from & candidate_pawns & second_back_rank) {
                                        move_list.emplace_back(from, active_attacker, Move::Promotion::queen_promo);
                                        move_list.emplace_back(from, active_attacker, Move::Promotion::rook_promo);
                                        move_list.emplace_back(from, active_attacker, Move::Promotion::horse_promo);
                                        move_list.emplace_back(from, active_attacker, Move::Promotion::bishop_promo);
                                } else if (is_en_passant) {
                                        const Field to = shifted<is_white ? north : south>(active_attacker);
                                        // move_list.emplace_back(from, *reinterpret_cast<const OneSquare *>(&to), Move::EnPassant);
                                        move_list.emplace_back(from, OneSquare_unsafe(to), Move::EnPassant);

                                }
                                #if 0 else if (from & king) {
                                        // the king may not capture a defended piece
                                        if ((active_attacker & pos.defend_map<other_col>()) == 0ull) {
                                                move_list.emplace_back(from, active_attacker);
                                        }
                                }
                                #endif
                                else {
                                        move_list.emplace_back(from, active_attacker);
                                }
                        }

                        // now we add the moves that block the attack
                        // this is of course not always possible, such as with a knight attack or if
                        // there is a piece right next to the king

                        if (block_area) for (const OneSquare &from : all_squares) {
                                if ((from & all_friendly) == 0ull)
                                        continue;

                                if (from & pos.pawns<col>()) {
                                        // 1 fwd, 2 fwd, en passant capture are all things that can block the check
                                        // normal captures can not

                                        // 1 fwd is always allowed
                                        // if the square ahead it in block_area, it is automatically free as well
                                        // const Field one_ahead_ = shifted<ahead>(from);
                                        // const OneSquare &one_ahead = *reinterpret_cast<const OneSquare *>(&one_ahead_);
                                        const OneSquare one_ahead = OneSquare_unsafe(shifted<ahead>(from));

                                        const bool pinned_one_ahead = pin_prevents(from, one_ahead);
                                        if (one_ahead & block_area && !pinned_one_ahead) {
                                                if (one_ahead & back_rank) {
                                                        move_list.emplace_back(from, one_ahead, Move::Promotion::queen_promo);
                                                        move_list.emplace_back(from, one_ahead, Move::Promotion::rook_promo);
                                                        move_list.emplace_back(from, one_ahead, Move::Promotion::horse_promo);
                                                        move_list.emplace_back(from, one_ahead, Move::Promotion::bishop_promo);
                                                } else {
                                                        move_list.emplace_back(from, one_ahead);
                                                }
                                        }

                                        if (from & second_rank && !pinned_one_ahead) {
                                                // we do not even have to check if this is pinned
                                                const Field two_ahead = shifted<ahead>(one_ahead);
                                                if ((one_ahead & total) == 0ull && two_ahead & block_area) {
                                                        // move_list.emplace_back(from, *reinterpret_cast<const OneSquare *>(&two_ahead));
                                                        move_list.emplace_back(from, OneSquare_unsafe(two_ahead));
                                                }
                                        } else if (from & en_passant_squares && !en_passant_pinned) {
                                                // en passant is an option
                                                // const Field to_ = shifted<ahead>(two_moved_pawn);
                                                // const OneSquare &to = *reinterpret_cast<const OneSquare *>(&to_);
                                                const OneSquare to = OneSquare_unsafe(shifted<ahead>(two_moved_pawn));
                                                if (!pin_prevents(from, to) && to & block_area) {
                                                        move_list.emplace_back(from, to, Move::EnPassant);
                                                }
                                        }
                                        continue;
                                }
                                Field available = 0ull;
                                if (from & pos.rooks<col>()) {
                                        available = get_weakly_blocked_straights(from, total);
                                } else if (from & pos.bishops<col>()) {
                                        available = get_weakly_blocked_diagonals(from, total);
                                } else if (from & pos.queen<col>()) {
                                        available = get_weakly_blocked_straights(from, total);
                                        available |= get_weakly_blocked_diagonals(from, total);
                                } else if (from & pos.horses<col>()) {
                                        available = get_horse_jumps(from);
                                }
                                available &= block_area;
                                for (const OneSquare &to : all_squares) {
                                        if ((to & available) == 0ull)
                                                continue;
                                        if (pin_prevents(from, to))
                                                continue;
                                        move_list.emplace_back(from, to);
                                }
                        }
                        // the other moves to save the king are evasions
                        // these also work if there are more attackers
                }

                // evasions are the only remaining option
                // of course the king cannot stop the attack ray, since he has to step aside
                Position pos_without_king = pos;
                pos_without_king.king<col>() ^= king;
                const Field defend_map = pos_without_king.defend_map<other_col>();
                const Field available  = get_king_area(king) & (~all_friendly) & (~defend_map);
                for (const OneSquare &to : all_squares) {
                        if (to & available) {
                                move_list.emplace_back(king, to);
                        }
                }
                // no other moves can be made
                return;
        }

        const Field straight_sliders = pos.rooks<col>()   | pos.queen<col>();
        const Field diagonal_sliders = pos.bishops<col>() | pos.queen<col>();

        // king is not in check, we make a normal move
        for (const OneSquare &from : all_squares) {
                if ((from & all_friendly) == 0ull)
                        continue;

                if (from & pos.pawns<col>()) {
                        // const Field one_ahead_ = shifted<ahead>(from);
                        // const OneSquare &one_ahead = *reinterpret_cast<const OneSquare *>(&one_ahead_);
                        const OneSquare one_ahead = OneSquare_unsafe(shifted<ahead>(from));

                        if ((total & one_ahead) == 0ull) {
                                const bool pinned_one_ahead = pin_prevents(from, one_ahead);
                                if (!pinned_one_ahead) {
                                        // now we add the move
                                        if (one_ahead & back_rank) {
                                                move_list.emplace_back(from, one_ahead, Move::Promotion::queen_promo);
                                                move_list.emplace_back(from, one_ahead, Move::Promotion::rook_promo);
                                                move_list.emplace_back(from, one_ahead, Move::Promotion::horse_promo);
                                                move_list.emplace_back(from, one_ahead, Move::Promotion::bishop_promo);
                                        } else {
                                                move_list.emplace_back(from, one_ahead);
                                        }
                                        // we can also attempt two ahead
                                        if (from & second_rank) {
                                                // const Field two_ahead_ = shifted<ahead>(one_ahead);
                                                // const OneSquare &two_ahead = *reinterpret_cast<const OneSquare *>(&two_ahead_);
                                                const OneSquare two_ahead = OneSquare_unsafe(shifted<ahead>(one_ahead));
                                                // no need to check for pins
                                                if ((total & two_ahead) == 0ull) {
                                                        move_list.emplace_back(from, two_ahead);
                                                }
                                        }
                                }
                        }
                        // captures, first just the normal ones
                        const Field lcapture_ = shifted<is_white ? northWest : southWest>(from);
                        const Field rcapture_ = shifted<is_white ? northEast : southEast>(from);

                        // if (lcapture_ & all_hostile && !pin_prevents(from, *reinterpret_cast<const OneSquare *>(&lcapture_))) {
                        if (lcapture_ & all_hostile && !pin_prevents(from, OneSquare_unsafe(lcapture_))) {

                                // const OneSquare &lcapture = *reinterpret_cast<const OneSquare *>(&lcapture_);
                                const OneSquare lcapture = OneSquare_unsafe(lcapture_);
                                if (lcapture & back_rank) {
                                        move_list.emplace_back(from, lcapture, Move::Promotion::queen_promo);
                                        move_list.emplace_back(from, lcapture, Move::Promotion::rook_promo);
                                        move_list.emplace_back(from, lcapture, Move::Promotion::horse_promo);
                                        move_list.emplace_back(from, lcapture, Move::Promotion::bishop_promo);
                                } else {
                                        move_list.emplace_back(from, lcapture);
                                }
                        }
                        // if (rcapture_ & all_hostile && !pin_prevents(from, *reinterpret_cast<const OneSquare *>(&rcapture_))) {
                        if (rcapture_ & all_hostile && !pin_prevents(from, OneSquare_unsafe(rcapture_))) {
                                // const OneSquare &rcapture = *reinterpret_cast<const OneSquare *>(&rcapture_);
                                const OneSquare rcapture = OneSquare_unsafe(rcapture_);

                                if (rcapture & back_rank) {
                                        move_list.emplace_back(from, rcapture, Move::Promotion::queen_promo);
                                        move_list.emplace_back(from, rcapture, Move::Promotion::rook_promo);
                                        move_list.emplace_back(from, rcapture, Move::Promotion::horse_promo);
                                        move_list.emplace_back(from, rcapture, Move::Promotion::bishop_promo);
                                } else {
                                        move_list.emplace_back(from, rcapture);
                                }
                        }

                        // en passant is subject to some more constraints
                        // most of these are already dealt with in en_passant_pinned
                        if (from & en_passant_squares && !en_passant_pinned) {
                                // const Field to_ = shifted<ahead>(two_moved_pawn);
                                // const OneSquare &to = *reinterpret_cast<const OneSquare *>(&to_);
                                const OneSquare to = OneSquare_unsafe(shifted<ahead>(two_moved_pawn));
                                if (!pin_prevents(from, to)) {
                                        move_list.emplace_back(from, to, Move::EnPassant);
                                }
                        }
                }
                if (from & pos.king<col>()) {
                        // obviously we cannot move into a check
                        // expensive routine call, but should only happen once anyway
                        const Field defend_map = pos.defend_map<other_col>();
                        const Field available  = get_king_area(king) & (~defend_map) & (~all_friendly);
                        for (const OneSquare &to : all_squares) {
                                if (to & available) {
                                        move_list.emplace_back(from, to);
                                }
                        }
                        // then there is the castling
                        constexpr Field queenside_empty = is_white ? white_castle_queen_freezone : black_castle_queen_freezone;
                        constexpr Field kingside_empty  = is_white ? white_castle_king_freezone : black_castle_king_freezone;
                        constexpr Field queenside_safe = is_white ? white_castle_queen_safezone : black_castle_queen_safezone;
                        constexpr Field kingside_safe  = is_white ? white_castle_king_safezone : black_castle_king_safezone;
                        constexpr Field queenside_rook = is_white ? white_queen_rook : black_queen_rook;
                        constexpr Field kingside_rook = is_white ? white_king_rook : black_king_rook;

                        const bool can_queen_castle = pos.meta.get_queen_castle<col>()
                                && (total & queenside_empty) == 0ull
                                && (defend_map & queenside_safe) == 0ull
                                && pos.rooks<col>() & queenside_rook;

                        const bool can_king_castle = pos.meta.get_king_castle<col>()
                                && (total & kingside_empty) == 0ull
                                && (defend_map & kingside_safe) == 0ull
                                && pos.rooks<col>() & kingside_rook;

                        if (can_queen_castle)
                                move_list.emplace_back(Move::CastleType::queenside);
                        if (can_king_castle)
                                move_list.emplace_back(Move::CastleType::kingside);
                        continue;
                }

                Field available = 0ull;
                if (from & pos.horses<col>()) {
                        available = get_horse_jumps(from);
                } else {
                        if (from & straight_sliders) {
                                available = get_weakly_blocked_straights(from, total);
                        }
                        if (from & diagonal_sliders) {
                                available |= get_weakly_blocked_diagonals(from, total);
                        }
                }
                available &= (~all_friendly);
                for (const OneSquare &to : all_squares) {
                        if ((to & available) == 0ull)
                                continue;

                        if (pin_prevents(from, to))
                                continue;

                        move_list.emplace_back(from, to);
                }
        }
}
*/

template <Color col>
inline
// auto generate_moves_sorted (const Position &pos, MoveList &move_list) -> void
auto generate_moves (const Position &pos, MoveList &move_list) -> void
{
        // not micro optimized

        // move sorting is important for the algorithm, so we sort
        // different kinds of moves into different lists
        // we merge them all before returning
        MoveList captures;
        MoveList blocks;
        MoveList quiets;

        // pushes the move onto the movelist, in an optimal order
        auto push_list = [&]() -> void {
                for (const Move capture : captures)
                        move_list.emplace_back(capture);
                for (const Move block : blocks)
                        move_list.emplace_back(block);
                for (const Move quiet : quiets)
                        move_list.emplace_back(quiet);
        };

        constexpr bool is_white = col == Color::white;
        constexpr Color other_col = !col;
        const Field all_friendly = pos.get_occupation<col>();
        const Field all_hostile  = pos.get_occupation<other_col>();
        const Field total = all_friendly | all_hostile;        // has all non-empty squares

        // todo this is ugly as hell

        // const Field king_ = pos.king<col>();
        // const OneSquare &king = *reinterpret_cast<const OneSquare *>(&king_);
        const OneSquare king = OneSquare_unsafe(pos.king<col>());
        // const Field atm = pos.attack_map<other_col>();

        constexpr Field en_passant_rank  = msk::rank[is_white ? 4 : 3];
        constexpr Field back_rank        = msk::rank[is_white ? 7 : 0];
        constexpr Field second_back_rank = msk::rank[is_white ? 6 : 1];
        constexpr Field second_rank      = msk::rank[is_white ? 1 : 6];
        constexpr Direction ahead = is_white ? north : south;

        // the pinned array contains for each direction,
        // the squares between the king and an active attacker, if any
        // or 0 if there is no attacker from that direction
        // if we capture the attacking piece, we're good too

        std::array<Field, 8> pinned = {};

        // the pieces that are causing the associated pins
        std::array<Field, 8> pin_causers = {};

        // there is a very special case where there are two pawns next to each other
        // with a king and enemy rook on each side
        // one pawn captures the other en passant, and the rook sees the king
        // this is such a problematic case that we just do this
        // there can only be one allowed en passant anyway
        bool en_passant_pinned = false;

        // is 0 if there is none
        const Field two_moved_pawn = msk::file[pos.meta.pawn2fwd_file()] & en_passant_rank;

        // contains the squares that can capture a pawn en passant, if any
        const Field en_passant_squares = shifted<east>(two_moved_pawn) | shifted<west>(two_moved_pawn);

        // we find the pins first
        const Field straight_attackers = pos.rooks<other_col>() | pos.queen<other_col>();
        const Field diagonal_attackers = pos.bishops<other_col>() | pos.queen<other_col>();

        auto calculate_pin = [&]<Direction dir> () {
                const Field &danger = is_straight(dir) ? straight_attackers : diagonal_attackers;
                const Field ray = get_weakly_blocked_ray<dir>(king, danger);

                if (ray & danger) {
                        const Field in_between = ray & ~danger;
                        // there is only a pin with 1 friendly and 0 hostile pieces
                        // unless the rare case with the en passant rook thing
                        // an en passant capture can also expose the king via a diagonal

                        const int num_friendly_between = bit_count(all_friendly & in_between);
                        const int num_enemy_between = bit_count(all_hostile & in_between);
                        if (num_friendly_between == 1 && num_enemy_between == 0) {
                                pinned[dir] = in_between;
                                pin_causers[dir] = ray & danger;
                        } else if (is_diagonal(dir) && num_friendly_between == 0 && num_enemy_between == 1 && (in_between & two_moved_pawn)) {
                                // we can not capture en passant, because that would expose the king
                                en_passant_pinned = true;
                        } else if constexpr (dir == east || dir == west) {

                                const bool is_special_case = num_friendly_between == 1
                                            && num_enemy_between == 1
                                            && two_moved_pawn & in_between // indeed a pawn in between that can be taken en passant
                                            && en_passant_squares & pos.pawns<col>() & in_between // friendly pawn next to the enemy pawn
                                            ; // redundant && king & en_passant_rank; // and the king has to be there as well

                                if (is_special_case) {
                                        en_passant_pinned = true;
                                }
                        }

                }
        };

        // returns true if this move is ILLEGAL due to a pin
        // does not take into account the special en passant case
        // it does take into account the normal en passant case
        auto pin_prevents = [&](const OneSquare &from, const OneSquare &to) -> bool {
                // if "from" is pinned, it can only move within the ray
                for (const Direction dir : directions) {
                        const Field &pinned_areas = pinned[dir];
                        const Field &pin_causer = pin_causers[dir];

                        if (from & pinned_areas) {
                                // if "to" is within the ray, we return false, since we can move within the pinned area
                                // if we capture the pinner, this is fine too

                                return (to & (pinned_areas | pin_causer)) == 0ull;
                        }
                }
                return false;
        };

        // thanks, Bjarne!
        calculate_pin.template operator()<north>();
        calculate_pin.template operator()<northEast>();
        calculate_pin.template operator()<northWest>();
        calculate_pin.template operator()<south>();
        calculate_pin.template operator()<southEast>();
        calculate_pin.template operator()<southWest>();
        calculate_pin.template operator()<west>();
        calculate_pin.template operator()<east>();

        const Field active_horse_attackers = pos.horses<other_col>() & get_horse_jumps(king);
        const Field active_diagonal_attackers = diagonal_attackers & get_weakly_blocked_diagonals(king, total);
        const Field active_straight_attackers = straight_attackers & get_weakly_blocked_straights(king, total);
        const Field active_pawn_attackers = [&]() -> Field {
                if constexpr (is_white)
                        return pos.pawns<other_col>() & (shifted<northEast>(king) | shifted<northWest>(king));
                return pos.pawns<other_col>() & (shifted<southEast>(king) | shifted<southWest>(king));
        }();

        const Field all_active_attackers = active_horse_attackers | active_diagonal_attackers | active_straight_attackers | active_pawn_attackers;

        // king is in check
        // we must do something about it
        if (all_active_attackers) {

                // depending on what attacks us, we generate moves
                // if there is only one attacker, capturing with an unpinned piece is an option
                // as well as blocking

                // evasion is always an option, also with multiple attackers
                const int num_attackers = bit_count(all_active_attackers);

                // if only one piece attacks the king, capturing or blocking is an option,
                // provided we respect pins
                // otherwise, we only have evasions

                if (num_attackers == 1) {
                        const Field active_attacker_ = [&]() -> Field {
                                if (active_diagonal_attackers)
                                        return active_diagonal_attackers;
                                if (active_straight_attackers)
                                        return active_straight_attackers;
                                if (active_horse_attackers)
                                        return active_horse_attackers;
                                return active_pawn_attackers;
                        }();

                        // ugly as hell, yes. Too bad!
                        // const OneSquare &active_attacker = *reinterpret_cast<const OneSquare *>(&active_attacker_);
                        const OneSquare active_attacker = OneSquare_unsafe(active_attacker_);

                        // find pieces of ours that can capture the attacker
                        // no pinned pieces of course

                        // squares where we can place a piece of ours to block
                        Field block_area     = 0ull;

                        Field weak_straights = 0ull;
                        Field weak_diags     = 0ull;

                        // const Field weak_straights = get_weakly_blocked_straights(active_attacker, total);
                        // const Field weak_diags     = get_weakly_blocked_diagonals(active_attacker, total);

                        // todo is this less eficient than just blindly checking king & ray?
                        bool found_king = false;

                        // routine that looks at a ray, starting from the attacker in a direction
                        // and if the king is there, sets the block_area
                        auto handle_ray = [&]<Direction dir>() -> void {
                                const Field ray = get_weakly_blocked_ray<dir>(active_attacker, total);
                                if (!found_king && king & ray) {
                                        found_king = true;
                                        block_area = ray & ~king;
                                }

                                if constexpr (is_straight(dir)) {
                                        weak_straights |= ray;
                                } else {
                                        weak_diags |= ray;
                                }
                        };

                        handle_ray.template operator()<north>();
                        handle_ray.template operator()<east>();
                        handle_ray.template operator()<south>();
                        handle_ray.template operator()<west>();
                        handle_ray.template operator()<northEast>();
                        handle_ray.template operator()<southEast>();
                        handle_ray.template operator()<southWest>();
                        handle_ray.template operator()<northWest>();

                        // here, block_area, weak_straights, weak_diags have been set
                        // the attack is blockable <==> block_area != 0;
                        const Field horse_area     = get_horse_jumps(active_attacker);
                        const Field pawn_area      = is_white ? (shifted<southEast>(active_attacker) | shifted<southWest>(active_attacker))
                                                              : (shifted<northEast>(active_attacker) | shifted<northWest>(active_attacker));
                        const Field king_area      = get_king_area(active_attacker) ;

                        // if the attacking piece is a pawn that just moved 2 forward, there are some additional places
                        // that a pawn can stand on to capture this pawn

                        // we can only capture en passant if this indeed removes the check
                        const Field en_passant_area   = (active_attacker & two_moved_pawn) ? en_passant_squares : 0ull;

                        // these are exactly the candidates in position to capture the attacker
                        const Field candidate_rooks   = pos.rooks<col>()   & weak_straights;
                        const Field candidate_bishops = pos.bishops<col>() & weak_diags;
                        const Field candidate_queens  = pos.queen<col>()   & (weak_diags | weak_straights);
                        const Field candidate_horses  = pos.horses<col>()  & horse_area;
                        const Field candidate_pawns   = pos.pawns<col>()   & pawn_area;
                        const Field candidate_kings   = pos.king<col>()    & king_area;
                        // we'll treat en passant captures as special cases
                        const Field candidate_en_passant_pawns = pos.pawns<col>() & en_passant_area;

                        const Field all_candidates = candidate_rooks  | candidate_bishops | candidate_queens
                                                   | candidate_horses | candidate_pawns   | candidate_kings | candidate_en_passant_pawns;


                        // first we add the moves that capture the attacker
                        // king moves here are counted as EVASIONS, because that is easier
                        for (const OneSquare &from : all_squares) {
                                bool is_en_passant = false;

                                if ((from & all_candidates) == 0ull)
                                        continue;

                                if (from & king)
                                        continue;

                                // we set the en passant flag if applicable
                                // en passant captures are pinned slightly differently
                                if (from & candidate_en_passant_pawns) {
                                        // const Field to_= shifted<ahead>(active_attacker);
                                        // const OneSquare &to = *reinterpret_cast<const OneSquare *>(&to_);
                                        const OneSquare to = OneSquare_unsafe(shifted<ahead>(active_attacker));

                                        if (en_passant_pinned || pin_prevents(from, to)) {
                                                continue;
                                        }
                                        is_en_passant = true;
                                        // the en passant pawn can be pinned as well
                                } else if (pin_prevents(from, active_attacker)) {
                                        // we still have to consider normal pins
                                        continue;
                                }

                                // now we add the move, since it is clearly allowed
                                if (from & candidate_pawns & second_back_rank) {
                                        captures.emplace_back(from, active_attacker, Move::Promotion::queen_promo);
                                        captures.emplace_back(from, active_attacker, Move::Promotion::rook_promo);
                                        captures.emplace_back(from, active_attacker, Move::Promotion::horse_promo);
                                        captures.emplace_back(from, active_attacker, Move::Promotion::bishop_promo);
                                } else if (is_en_passant) {
                                        const Field to = shifted<is_white ? north : south>(active_attacker);
                                        // move_list.emplace_back(from, *reinterpret_cast<const OneSquare *>(&to), Move::EnPassant);
                                        captures.emplace_back(from, OneSquare_unsafe(to), Move::EnPassant);

                                } /* else if (from & king) {
                                        // the king may not capture a defended piece
                                        if ((active_attacker & pos.defend_map<other_col>()) == 0ull) {
                                                move_list.emplace_back(from, active_attacker);
                                        }
                                } */
                                else {
                                        move_list.emplace_back(from, active_attacker);
                                }
                        }

                        // now we add the moves that block the attack
                        // this is of course not always possible, such as with a knight attack or if
                        // there is a piece right next to the king


                        if (block_area) for (const OneSquare &from : all_squares) {
                                if ((from & all_friendly) == 0ull)
                                        continue;

                                if (from & pos.pawns<col>()) {
                                        // 1 fwd, 2 fwd, en passant capture are all things that can block the check
                                        // normal captures can not

                                        // 1 fwd is always allowed
                                        // if the square ahead it in block_area, it is automatically free as well
                                        // const Field one_ahead_ = shifted<ahead>(from);
                                        // const OneSquare &one_ahead = *reinterpret_cast<const OneSquare *>(&one_ahead_);
                                        const OneSquare one_ahead = OneSquare_unsafe(shifted<ahead>(from));

                                        const bool pinned_one_ahead = pin_prevents(from, one_ahead);
                                        if (one_ahead & block_area && !pinned_one_ahead) {
                                                if (one_ahead & back_rank) {
                                                        blocks.emplace_back(from, one_ahead, Move::Promotion::queen_promo);
                                                        blocks.emplace_back(from, one_ahead, Move::Promotion::rook_promo);
                                                        blocks.emplace_back(from, one_ahead, Move::Promotion::horse_promo);
                                                        blocks.emplace_back(from, one_ahead, Move::Promotion::bishop_promo);
                                                } else {
                                                        blocks.emplace_back(from, one_ahead);
                                                }
                                        }

                                        if (from & second_rank && !pinned_one_ahead) {
                                                // we do not even have to check if this is pinned
                                                const Field two_ahead = shifted<ahead>(one_ahead);
                                                if ((one_ahead & total) == 0ull && two_ahead & block_area) {
                                                        // move_list.emplace_back(from, *reinterpret_cast<const OneSquare *>(&two_ahead));
                                                        blocks.emplace_back(from, OneSquare_unsafe(two_ahead));
                                                }
                                        } else if (from & en_passant_squares && !en_passant_pinned) {
                                                // en passant is an option
                                                const OneSquare to = OneSquare_unsafe(shifted<ahead>(two_moved_pawn));
                                                if (!pin_prevents(from, to) && to & block_area) {
                                                        blocks.emplace_back(from, to, Move::EnPassant);
                                                }
                                        }
                                        continue;
                                }
                                Field available = 0ull;
                                if (from & pos.rooks<col>()) {
                                        available = get_weakly_blocked_straights(from, total);
                                } else if (from & pos.bishops<col>()) {
                                        available = get_weakly_blocked_diagonals(from, total);
                                } else if (from & pos.queen<col>()) {
                                        available = get_weakly_blocked_straights(from, total);
                                        available |= get_weakly_blocked_diagonals(from, total);
                                } else if (from & pos.horses<col>()) {
                                        available = get_horse_jumps(from);
                                }
                                available &= block_area;
                                for (const OneSquare &to : all_squares) {
                                        if ((to & available) == 0ull)
                                                continue;
                                        if (pin_prevents(from, to))
                                                continue;
                                        blocks.emplace_back(from, to);
                                }
                        }
                        // the other moves to save the king are evasions
                        // these also work if there are more attackers
                }

                // evasions are the only remaining option
                // of course the king cannot stop the attack ray, since he has to step aside
                Position pos_without_king = pos;
                pos_without_king.king<col>() ^= king;
                const Field defend_map = pos_without_king.defend_map<other_col>();
                const Field available  = get_king_area(king) & (~all_friendly) & (~defend_map);
                for (const OneSquare &to : all_squares) {
                        if (to & available) {
                                quiets.emplace_back(king, to);
                        }
                }
                // no other moves can be made
                push_list();
                return;
        }

        const Field straight_sliders = pos.rooks<col>()   | pos.queen<col>();
        const Field diagonal_sliders = pos.bishops<col>() | pos.queen<col>();

        // king is not in check, we make a normal move
        for (const OneSquare &from : all_squares) {
                if ((from & all_friendly) == 0ull)
                        continue;

                if (from & pos.pawns<col>()) {
                        // const Field one_ahead_ = shifted<ahead>(from);
                        // const OneSquare &one_ahead = *reinterpret_cast<const OneSquare *>(&one_ahead_);
                        const OneSquare one_ahead = OneSquare_unsafe(shifted<ahead>(from));

                        if ((total & one_ahead) == 0ull) {
                                const bool pinned_one_ahead = pin_prevents(from, one_ahead);
                                if (!pinned_one_ahead) {
                                        // now we add the move
                                        if (one_ahead & back_rank) {
                                                quiets.emplace_back(from, one_ahead, Move::Promotion::queen_promo);
                                                quiets.emplace_back(from, one_ahead, Move::Promotion::rook_promo);
                                                quiets.emplace_back(from, one_ahead, Move::Promotion::horse_promo);
                                                quiets.emplace_back(from, one_ahead, Move::Promotion::bishop_promo);
                                        } else {
                                                quiets.emplace_back(from, one_ahead);
                                        }
                                        // we can also attempt two ahead
                                        if (from & second_rank) {
                                                // const Field two_ahead_ = shifted<ahead>(one_ahead);
                                                // const OneSquare &two_ahead = *reinterpret_cast<const OneSquare *>(&two_ahead_);
                                                const OneSquare two_ahead = OneSquare_unsafe(shifted<ahead>(one_ahead));
                                                // no need to check for pins
                                                if ((total & two_ahead) == 0ull) {
                                                        quiets.emplace_back(from, two_ahead);
                                                }
                                        }
                                }
                        }
                        // captures, first just the normal ones
                        const Field lcapture_ = shifted<is_white ? northWest : southWest>(from);
                        const Field rcapture_ = shifted<is_white ? northEast : southEast>(from);

                        // if (lcapture_ & all_hostile && !pin_prevents(from, *reinterpret_cast<const OneSquare *>(&lcapture_))) {
                        if (lcapture_ & all_hostile && !pin_prevents(from, OneSquare_unsafe(lcapture_))) {

                                const OneSquare lcapture = OneSquare_unsafe(lcapture_);
                                if (lcapture & back_rank) {
                                        captures.emplace_back(from, lcapture, Move::Promotion::queen_promo);
                                        captures.emplace_back(from, lcapture, Move::Promotion::rook_promo);
                                        captures.emplace_back(from, lcapture, Move::Promotion::horse_promo);
                                        captures.emplace_back(from, lcapture, Move::Promotion::bishop_promo);
                                } else {
                                        captures.emplace_back(from, lcapture);
                                }
                        }
                        if (rcapture_ & all_hostile && !pin_prevents(from, OneSquare_unsafe(rcapture_))) {
                                const OneSquare rcapture = OneSquare_unsafe(rcapture_);

                                if (rcapture & back_rank) {
                                        captures.emplace_back(from, rcapture, Move::Promotion::queen_promo);
                                        captures.emplace_back(from, rcapture, Move::Promotion::rook_promo);
                                        captures.emplace_back(from, rcapture, Move::Promotion::horse_promo);
                                        captures.emplace_back(from, rcapture, Move::Promotion::bishop_promo);
                                } else {
                                        captures.emplace_back(from, rcapture);
                                }
                        }

                        // en passant is subject to some more constraints
                        // most of these are already dealt with in en_passant_pinned
                        if (from & en_passant_squares && !en_passant_pinned) {
                                // const Field to_ = shifted<ahead>(two_moved_pawn);
                                // const OneSquare &to = *reinterpret_cast<const OneSquare *>(&to_);
                                const OneSquare to = OneSquare_unsafe(shifted<ahead>(two_moved_pawn));
                                if (!pin_prevents(from, to)) {
                                        captures.emplace_back(from, to, Move::EnPassant);
                                }
                        }
                }
                if (from & pos.king<col>()) {
                        // obviously we cannot move into a check
                        // expensive routine call, but should only happen once anyway
                        const Field defend_map = pos.defend_map<other_col>();
                        const Field available  = get_king_area(king) & (~defend_map) & (~all_friendly);
                        for (const OneSquare &to : all_squares) {
                                if (to & available) {
                                        if (to & all_hostile) {
                                                captures.emplace_back(from, to);
                                        } else {
                                                quiets.emplace_back(from, to);
                                        }
                                }
                        }
                        // then there is the castling
                        constexpr Field queenside_empty = is_white ? white_castle_queen_freezone : black_castle_queen_freezone;
                        constexpr Field kingside_empty  = is_white ? white_castle_king_freezone : black_castle_king_freezone;
                        constexpr Field queenside_safe = is_white ? white_castle_queen_safezone : black_castle_queen_safezone;
                        constexpr Field kingside_safe  = is_white ? white_castle_king_safezone : black_castle_king_safezone;
                        constexpr Field queenside_rook = is_white ? white_queen_rook : black_queen_rook;
                        constexpr Field kingside_rook = is_white ? white_king_rook : black_king_rook;

                        const bool can_queen_castle = pos.meta.get_queen_castle<col>()
                                && (total & queenside_empty) == 0ull
                                && (defend_map & queenside_safe) == 0ull
                                && pos.rooks<col>() & queenside_rook;

                        const bool can_king_castle = pos.meta.get_king_castle<col>()
                                && (total & kingside_empty) == 0ull
                                && (defend_map & kingside_safe) == 0ull
                                && pos.rooks<col>() & kingside_rook;

                        if (can_queen_castle)
                                quiets.emplace_back(Move::CastleType::queenside);
                        if (can_king_castle)
                                quiets.emplace_back(Move::CastleType::kingside);
                        continue;
                }

                Field available = 0ull;
                if (from & pos.horses<col>()) {
                        available = get_horse_jumps(from);
                } else {
                        if (from & straight_sliders) {
                                available = get_weakly_blocked_straights(from, total);
                        }
                        if (from & diagonal_sliders) {
                                available |= get_weakly_blocked_diagonals(from, total);
                        }
                }
                available &= (~all_friendly);
                for (const OneSquare &to : all_squares) {
                        if ((to & available) == 0ull)
                                continue;

                        if (pin_prevents(from, to))
                                continue;

                        if (to & all_hostile) {
                                captures.emplace_back(from, to);
                        } else {
                                quiets.emplace_back(from, to);
                        }
                }
        }

        push_list();
}

#endif //MOVE_GEN_H
