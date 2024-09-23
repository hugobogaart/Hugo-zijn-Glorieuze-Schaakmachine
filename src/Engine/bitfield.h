//
// Created by Hugo Bogaart on 19/07/2024.
//

#ifndef BITFIELD_H
#define BITFIELD_H

#include <cstdint>
#include "gen-defs.h"
#include <array>

// todo and file 8 for "none" is sketchy
// todo type ranks and files

// for the table generator
// #define ALL_CALC
#ifdef ALL_CALC
constexpr CalculationType free_ray_ct    = CalculationType::calculation;
constexpr CalculationType w_blocked_ray_ct = CalculationType::calculation;
constexpr CalculationType free_straights_ct = CalculationType::calculation;
constexpr CalculationType free_diagonals_ct = CalculationType::calculation;
constexpr CalculationType w_blocked_diagonals_ct = CalculationType::calculation;
constexpr CalculationType w_blocked_straights_ct  = CalculationType::calculation;

#else
constexpr CalculationType free_ray_ct    = CalculationType::calculation;
constexpr CalculationType w_blocked_ray_ct = CalculationType::calculation;

constexpr CalculationType free_straights_ct = CalculationType::lookup_table;
constexpr CalculationType free_diagonals_ct = CalculationType::lookup_table;

constexpr CalculationType w_blocked_diagonals_ct = CalculationType::lookup_table;
constexpr CalculationType w_blocked_straights_ct  = CalculationType::lookup_table;

#endif

// some combinations of CalculationTypes are stupid, such as calculating quadrants while looking up halves
static_assert((is_calculation(w_blocked_ray_ct) && is_lookup(free_ray_ct)) == false, "calculating blocked rays and looking up free rays is dumb\n");


typedef uint64_t Field; // to interpret as 8x8 boards in rank major order
// 1 == a1
// 2 == a2
// 1 << 8 = b1 etc.

// some useful bitmask  s
namespace msk {

        // margin of 1
        // &= LEFT removes all the left side bits, etc
        constexpr Field RIGHT  = 0b0111111101111111011111110111111101111111011111110111111101111111; // zeros on the right side
        constexpr Field LEFT   = 0b1111111011111110111111101111111011111110111111101111111011111110; // zeros on the left side
        constexpr Field TOP    = 0b0000000011111111111111111111111111111111111111111111111111111111; // zeros above
        constexpr Field BOTTOM = 0b1111111111111111111111111111111111111111111111111111111100000000; // zeros below

        // margin of 2
        constexpr Field RIGHT2  = 0b0011111100111111001111110011111100111111001111110011111100111111;
        constexpr Field LEFT2   = 0b1111110011111100111111001111110011111100111111001111110011111100;
        constexpr Field TOP2    = 0b0000000000000000111111111111111111111111111111111111111111111111;
        constexpr Field BOTTOM2 = 0b1111111111111111111111111111111111111111111111110000000000000000;

        constexpr Field white_squares = 0b0101010110101010010101011010101001010101101010100101010110101010;
        constexpr Field black_squares = ~white_squares;

        // lookup table to see if a point is in a rank
        // so field & rank[3] == true <==> field has a bit in rank 3
        constexpr Field rank[8] {
            0b0000000000000000000000000000000000000000000000000000000011111111,
            0b0000000000000000000000000000000000000000000000001111111100000000,
            0b0000000000000000000000000000000000000000111111110000000000000000,
            0b0000000000000000000000000000000011111111000000000000000000000000,
            0b0000000000000000000000001111111100000000000000000000000000000000,
            0b0000000000000000111111110000000000000000000000000000000000000000,
            0b0000000011111111000000000000000000000000000000000000000000000000,
            0b1111111100000000000000000000000000000000000000000000000000000000
        };

        // and also for files
        constexpr Field file[9] {
                0b0000000100000001000000010000000100000001000000010000000100000001,
                0b0000001000000010000000100000001000000010000000100000001000000010,
                0b0000010000000100000001000000010000000100000001000000010000000100,
                0b0000100000001000000010000000100000001000000010000000100000001000,
                0b0001000000010000000100000001000000010000000100000001000000010000,
                0b0010000000100000001000000010000000100000001000000010000000100000,
                0b0100000001000000010000000100000001000000010000000100000001000000,
                0b1000000010000000100000001000000010000000100000001000000010000000,
                0 // when en passant is nowhere allowed, the value of the en passant file is 8
                  // for convenience
        };

        constexpr Field edges = file[0] | file[7] | rank[0] | rank[7];

        // the edges going in
        constexpr Field ring[4] = {
                edges,
                (file[1] | file[6] | rank[1] | rank[6]) & ~ring[0],
                (file[2] | file[5] | rank[2] | rank[5]) & ~(ring[0] | ring[1]),
                (file[3] | file[4] | rank[3] | rank[4]) & ~(ring[0] | ring[1] | ring[2])
        };

}

/*
enum class Rank {};
enum class File {};
enum class Square {};
 */



// field with guarantee of having only one bit
// could technically be stored as a 6 bit integer
struct OneSquare {

        constexpr
        operator Field() const { return fd; }

        constexpr
        OneSquare (uint8_t rank, uint8_t file);

        constexpr friend
        auto square_from_shift (int sh) -> OneSquare;

        constexpr
        OneSquare();

        Field fd;

        constexpr friend
        auto OneSquare_unsafe (Field f) -> OneSquare;

private:
        constexpr explicit
        OneSquare (Field f) : fd(f) {}


};
// array all_squares is defined

constexpr
auto square_from_shift (int sh) -> OneSquare;

constexpr
auto square_to_shift (OneSquare sq) -> int;

constexpr
auto OneSquare_unsafe (Field f) -> OneSquare;

// the king/single-tile moves
// used for shifts
enum Direction : uint8_t {
        north,
        northEast,
        east,
        southEast,
        south,
        southWest,
        west,
        northWest
};

constexpr std::array directions = {
        north,
        northEast,
        east,
        southEast,
        south,
        southWest,
        west,
        northWest
};

enum HorseDirection : uint8_t {
        NNE,
        NEE,
        SEE,
        SSE,
        SSW,
        SWW,
        NWW,
        NNW
};

constexpr
auto flip_dir (Direction dir) -> Direction;

constexpr
auto flip_horsedir (HorseDirection hdir) -> HorseDirection;

constexpr auto is_diagonal (Direction dir) -> bool;

constexpr auto is_straight (Direction dir) -> bool;
// if point = 1ull << sh, returns the rank of the point
constexpr
auto shift_to_rank (int bshift) -> uint8_t;

// if point = 1ull << sh, returns the rank of point
constexpr
auto shift_to_file (int bshift) -> uint8_t;

// returns the file of the bit in point
constexpr
auto square_file(const OneSquare &point) -> uint8_t;

// returns the rank of the given field
constexpr
auto square_rank (const OneSquare &point) -> uint8_t;


/*
 * shifts a given field by one tile in one direction
 * will make zero if the number falls off
 */
template <Direction dir>
constexpr
void shift(Field &fd);
/* only specializations are implemented */

// returns new one instead
template <Direction dir>
constexpr
Field shifted(const Field &f);


/*
 * shifts a given field by a horse jump in specified direction
 * will make zero if the number falls off
 */
template <HorseDirection dir>
constexpr
void shift_horse(Field &fd);

template <HorseDirection dir>
constexpr
Field shifted_horse(const Field &f);

// shift, but with parameter
constexpr
auto var_shift(Direction dir, Field &f) -> void;

// extends a piece into a direction
// ray does not include own square
template <Direction dir>
auto get_free_ray (const OneSquare &point) -> Field;

// also selects the square with the obstacle
template <Direction dir>
auto get_weakly_blocked_ray(const OneSquare &point, const Field &weak) -> Field;

// for bishop moves
inline
auto get_free_diagonals (const OneSquare &point) -> Field;

// weakly blocked
inline
auto get_weakly_blocked_diagonals (const OneSquare &point, const Field &weak) -> Field;

// for rook moves
inline
auto get_free_straights(const OneSquare &point) -> Field;

// weakly blocked
inline
auto get_weakly_blocked_straights (const OneSquare &point, const Field &weak) -> Field;


//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
/// IMPLEMENTATION /// IMPLEMENTATION /// IMPLEMENTATION /// IMPLEMENTATION /// IMPLEMENTATION ///
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr
auto shift_to_rank (int bshift) -> uint8_t
{
        // return bshift / 8;
        return bshift >> 3;
}

constexpr
auto shift_to_file (int bshift) -> uint8_t
{
        // return bshift % 8;
        return bshift & 0b111;
}

// returns the file of the given point
constexpr
auto square_file (const OneSquare &point) -> uint8_t
{
        // return trailing_0_count(point) % 8;
        return shift_to_file(trailing_0_count(point));
}

// returns the rank of the given field
constexpr
auto square_rank(const OneSquare &point) -> uint8_t
{
        // return trailing_0_count(point) / 8;
        return shift_to_rank(trailing_0_count(point));
}

// creates a field with a bit on the specified rank and file
constexpr
OneSquare::OneSquare(uint8_t rank, uint8_t file)
        : fd(msk::rank[rank] & msk::file[file])
{
}

constexpr
OneSquare::OneSquare()
        : fd(1)
{
}

constexpr
auto square_from_shift (int sh) -> OneSquare
{
        return OneSquare(1ull << sh);
}

constexpr
auto square_to_shift (OneSquare sq) -> int
{
        return trailing_0_count(sq);
}

constexpr
auto OneSquare_unsafe (Field f) -> OneSquare
{
        OneSquare sq(f);
        return sq;
}

template <>
constexpr
void shift<north>(Field &fd)
{
        fd <<= 8;
        // no overflow protection necessary
        // bit will simply fall away
}

template <>
constexpr
void shift<northEast>(Field &fd)
{
        // there is overflow if the number ends up on the left again
        fd <<= 9;

        // to enforce it will go to zero with overflow
        fd &= msk::LEFT;
        // no overflow on the top
}

template <>
constexpr
void shift<east>(Field &fd)
{
        fd <<= 1;
        fd &= msk::LEFT;
}

template <>
constexpr
void shift<southEast>(Field &fd)
{
        fd >>= 7;
        fd &= msk::LEFT;
}

template <>
constexpr
void shift<south>(Field &fd)
{
        fd >>= 8;
        // no need to check, underflow
}

template <>
constexpr
void shift<southWest>(Field &fd)
{
        fd >>= 9;
        fd &= msk::RIGHT;
}

template <>
constexpr
void shift<west>(Field &fd)
{
        fd >>= 1;
        fd &= msk::RIGHT;
}

template <>
constexpr
void shift<northWest>(Field &fd)
{
        fd <<= 7;
        fd &= msk::RIGHT;
}

template <>
constexpr
void shift_horse<NNE>(Field &fd)
{
        fd <<= 17;
        fd &= msk::LEFT;
}

template <>
constexpr
void shift_horse<NEE>(Field &fd)
{
        fd <<= 10;
        fd &= msk::LEFT2;
}

template <>
constexpr
void shift_horse<SEE>(Field &fd)
{
        fd >>= 6;
        fd &= msk::LEFT2;
}

template <>
constexpr
void shift_horse<SSE>(Field &fd)
{
        fd >>= 15;
        fd &= msk::LEFT;
}

template <>
constexpr
void shift_horse<SSW>(Field &fd)
{
        fd >>= 17;
        fd &= msk::RIGHT;
}

template <>
constexpr
void shift_horse<SWW>(Field &fd)
{
        fd >>= 10;
        fd &= msk::RIGHT2;
}

template <>
constexpr
void shift_horse<NWW>(Field &fd)
{
        fd <<= 6;
        fd &= msk::RIGHT2;
}

template <>
constexpr
void shift_horse<NNW>(Field &fd)
{
        fd <<= 15;
        fd &= msk::RIGHT;
}

template <Direction dir>
constexpr
Field shifted(const Field &f)
{
        Field copy = f;
        shift<dir>(copy);
        return copy;
}

template <HorseDirection dir>
constexpr
Field shifted_horse(const Field &f)
{
        Field copy = f;
        shift_horse<dir>(copy);
        return copy;
}

// shift, but with parameter
constexpr
auto var_shift(Direction dir, Field &f) -> void
{
        switch (dir) {
        case north:     shift<north>(f);        break;
        case northEast: shift<northEast>(f);    break;
        case east:      shift<east>(f);         break;
        case southEast: shift<southEast>(f);    break;
        case south:     shift<south>(f);        break;
        case southWest: shift<southWest>(f);    break;
        case west:      shift<west>(f);         break;
        case northWest: shift<northWest>(f);    break;
        }
}


constexpr
auto flip_dir (Direction dir) -> Direction
{
        return static_cast<Direction>((dir + 4) % 8);
}

constexpr
auto flip_horsedir (HorseDirection hdir) -> HorseDirection
{
        return static_cast<HorseDirection>((hdir + 4) % 8);
}

constexpr auto is_straight (Direction dir) -> bool
{
        return dir == north || dir == south || dir == west || dir == east;
}

constexpr auto is_diagonal (Direction dir) -> bool
{
        return !is_straight(dir);
}

///
/// most of these functions have two versions
/// pure calculation, and table lookup
///

extern const std::array<Field, 64 * 8> free_ray_lookup_table;

template <Direction dir>
auto get_free_ray_calc (const OneSquare &point) -> Field
{
        Field ray = 0;
        Field pt = shifted<dir>(point);
        while (pt) {
                ray |= pt;
                shift<dir>(pt);
        }
        return ray;
}

// ray does not include own square
template <Direction dir>
auto get_free_ray (const OneSquare &point) -> Field
{
        if constexpr (free_ray_ct == CalculationType::lookup_table) {
                const int shift = square_to_shift(point);
                return free_ray_lookup_table[dir * 64 + shift];
        } else /* calculation */ {
                return get_free_ray_calc<dir>(point);
        }
}

// uses weak obstacles
// also puts all directions in the same array
extern const std::array<Field, 64 * 64 * 8> weakly_blocked_ray_lookup_table;


template <Direction dir>
auto get_weakly_blocked_ray_calc (const OneSquare &point, const Field &weak_obstacles) -> Field
{
        Field ray = 0;
        Field pt = shifted<dir>(point);
        while (pt && !(pt & weak_obstacles)) {
                ray |= pt;
                shift<dir>(pt);
        }
        ray |= pt;   // also ok if pt has gone out of the border
        return ray;
}


// like get_blocked_ray, but also selects the square with the obstacle
template <Direction dir>
auto get_weakly_blocked_ray(const OneSquare &point, const Field &weak) -> Field
{
        if constexpr (w_blocked_ray_ct == CalculationType::lookup_table) {
                /*
                // a weak obstacle is the same as a normal obstacle, but shifted over

                const Field mask = get_free_ray<dir>(point);
                const Field strong_obstacles = shifted<dir>(weak);

                const size_t obstacle_idx = pext(strong_obstacles, mask);
                const int shift = square_to_shift(point);
                return blocked_ray_lookup_table<dir>[shift * 128 + obstacle_idx];
                */
                const int shift = square_to_shift(point);
                const int file  = shift_to_file(shift);
                const int rank  = shift_to_rank(shift);
                Field relevant_bits = get_free_ray<dir>(point);
                if (file != 0)
                        relevant_bits &= ~msk::file[0];
                if (file != 7)
                        relevant_bits &= ~msk::file[7];
                if (rank != 0)
                        relevant_bits &= ~msk::rank[0];
                if (rank != 7)
                        relevant_bits &= ~msk::rank[7];
                const size_t obstacle_idx = pext(weak, relevant_bits);
                return weakly_blocked_ray_lookup_table[dir * 64 * 64 + shift * 64 + obstacle_idx];
        } else /* calculation */ {
                return get_weakly_blocked_ray_calc<dir>(point, weak);
        }
}

// for bishop moves
extern const std::array<Field, 64> free_diagonals_lookup_table;

inline
auto get_free_diagonals_calc (const OneSquare &point) -> Field
{
        return get_free_ray<northWest>(point) | get_free_ray<northEast>(point)
             | get_free_ray<southWest>(point) | get_free_ray<southEast>(point);
}

// does not include the point
inline
auto get_free_diagonals(const OneSquare &point) -> Field
{
        if constexpr (free_diagonals_ct == CalculationType::lookup_table) {
                const int shift = square_to_shift(point);
                return free_diagonals_lookup_table[shift];
        } else /* calculation */{
                return get_free_diagonals_calc(point);
        }
}


// we can wih a lookup table find (weak obstacles) -> (equivalent strong pieces)
// since weak obstacles on the edges do not matter, we only have 2^9 = 512 configurations to look at
constexpr size_t max_weak_diagonal_configurations = 512;
extern const std::array<Field, 64 * max_weak_diagonal_configurations> diagonals_weak_to_strong_lookup_table;

// there are at most 13 pieces on diagonals, but the edges don't matter
// there are at most 9 tiles with relevant weak blockers
constexpr size_t max_w_diagonal_configs = 1 << 9;
extern const std::array<Field, 64 * max_w_diagonal_configs> weakly_blocked_diagonals_lookup_table;


inline
auto get_weakly_blocked_diagonals_calc (const OneSquare &point, const Field &weak) -> Field
{
        return get_weakly_blocked_ray<northWest>(point, weak)
                     | get_weakly_blocked_ray<northEast>(point, weak)
                     | get_weakly_blocked_ray<southWest>(point, weak)
                     | get_weakly_blocked_ray<southEast>(point, weak);
}

inline
auto get_weakly_blocked_diagonals (const OneSquare &point, const Field &weak) -> Field
{
        if constexpr (w_blocked_diagonals_ct == CalculationType::lookup_table) {
                const int shift = square_to_shift(point);
                const Field relevant_bits = get_free_diagonals(point) & ~msk::edges;
                const size_t obstacle_idx = pext(weak, relevant_bits);
                return weakly_blocked_diagonals_lookup_table[shift * max_w_diagonal_configs + obstacle_idx];
        } else /* calculation */ {
                return get_weakly_blocked_diagonals_calc(point, weak);
        }
}

// for rook moves
extern const std::array<Field, 64> free_straights_lookup_table;

inline
auto get_free_straights_calc (const OneSquare &point) -> Field
{
        const int rank = square_rank(point);
        const int file = square_file(point);
        return (msk::file[file] | msk::rank[rank]) ^ point;
}

// does not include the point in the rays
inline
auto get_free_straights(const OneSquare &point) -> Field
{
        if constexpr (free_straights_ct == CalculationType::calculation) {
                return get_free_straights_calc(point);
        } else {
                const int shift = square_to_shift(point);
                return free_straights_lookup_table[shift];
        }
}

// there are at most 14 pieces on straights, but at least two are always irrelevant
// so 2^12 == 16384 possible configurations
constexpr size_t max_w_straight_configs = 1 << 12;
extern const std::array<Field, 64 * max_w_straight_configs> weakly_blocked_straights_lookup_table;

inline
auto get_weakly_blocked_straights_calc (const OneSquare &point, const Field &weak) -> Field
{
        Field attack = 0;
        attack |= get_weakly_blocked_ray<north>(point, weak);
        attack |= get_weakly_blocked_ray<east>(point, weak);
        attack |= get_weakly_blocked_ray<west>(point, weak);
        attack |= get_weakly_blocked_ray<south>(point, weak);
        return attack;
}

inline
auto get_weakly_blocked_straights (const OneSquare &point, const Field &weak) -> Field
{
        if constexpr (w_blocked_straights_ct == CalculationType::lookup_table) {
                const int shift = square_to_shift(point);

                // if the rook is on the edge it can still move along the edge
                // so we cannot blindly remove the edges
                Field relevant_bits = get_free_straights(point);
                const int file = shift_to_file(shift);
                const int rank = shift_to_rank(shift);

                if (file != 0)
                        relevant_bits &= ~msk::file[0];
                if (file != 7)
                        relevant_bits &= ~msk::file[7];
                if (rank != 0)
                        relevant_bits &= ~msk::rank[0];
                if (rank != 7)
                        relevant_bits &= ~msk::rank[7];


                const size_t obstacle_idx = pext(weak, relevant_bits);
                return weakly_blocked_straights_lookup_table[shift * max_w_straight_configs + obstacle_idx];
        } else /* calculation */ {
                return get_weakly_blocked_straights_calc(point, weak);
        }
}

constexpr std::array<OneSquare, 64> all_squares = []() constexpr {
        OneSquare arr[64];
        for (int i = 0; i < 64; i++)
                arr[i] = square_from_shift(i);
        return std::to_array(arr);
}();

#endif //BITFIELD_H
