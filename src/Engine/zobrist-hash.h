//
// Created by Hugo Bogaart on 29/07/2024.
//

#ifndef ZOBRIN_HASH_H
#define ZOBRIN_HASH_H

#include <cstdint>
#include <array>
#include "position.h"


namespace HashConstants
{
        // paranoid, the last bit in the hash is reserved for active_color color

        // monadic way of generating pseudo random numbers
        constexpr uint64_t seed = 3348105;

        struct PseudoRandomResult {
                uint64_t next_seed = 0;
                uint64_t result = 0;
        };

        constexpr
        auto pseudorandom (uint64_t seed) -> PseudoRandomResult
        {
                seed ^= seed >> 12;
                seed ^= seed << 25;
                seed ^= seed >> 27;

                PseudoRandomResult res;
                res.next_seed = seed;
                // res.result = seed * 0x2545F4914F6CDD1Dull;
                res.result = (seed ^ seed << 33) & ~1ull;
                return res;
        }

        template <size_t N>
        struct PseudoRandomArrayResult {
                uint64_t next_seed = 0;
                std::array<uint64_t, N> result = {};
        };

        template <size_t N>
        constexpr
        auto random_array (uint64_t seed) -> PseudoRandomArrayResult<N>
        {
                // chain the function using the seed as input for the next
                PseudoRandomArrayResult<N> res;
                std::generate_n(res.result.begin(), N, [&] {
                        PseudoRandomResult subres = pseudorandom(seed);
                        seed = subres.next_seed;
                        return subres.result;
                });
                res.next_seed = seed;
                return res;
        }

        namespace sub
        {
                constexpr PseudoRandomArrayResult<12 * 64> piece_square_randoms_inter = random_array<12 * 64>(seed);
                // seed ->
                constexpr PseudoRandomResult black_move_random_inter = pseudorandom(piece_square_randoms_inter.next_seed);
                // seed ->
                constexpr PseudoRandomArrayResult<16> castling_right_randoms_inter = random_array<16>(black_move_random_inter.next_seed);
                // seed ->
                constexpr PseudoRandomArrayResult<8> en_passant_randoms_inter = random_array<8>(castling_right_randoms_inter.next_seed);
        }

        constexpr std::array<uint64_t, 12 * 64> piece_square_randoms = sub::piece_square_randoms_inter.result;
        constexpr uint64_t black_move_hash = 1; // sub::black_move_random_inter.result;
        constexpr std::array<uint64_t, 16> castling_right_randoms = sub::castling_right_randoms_inter.result;
        constexpr std::array<uint64_t, 8> en_passant_randoms = sub::en_passant_randoms_inter.result;

        constexpr auto piece_square_hash (Epiece pc, int sh) -> uint64_t {return piece_square_randoms[64 * pc + sh];}

        /*
        enum CastleType {
                white_queen = 1 << 3, black_queen = 1 << 2, white_king = 1 << 1, black_king = 1
        };
        */

        // crights has to be in the format of MetaData
        constexpr auto castling_right_hash (uint8_t crights)
        {
                return castling_right_randoms[crights];
        }

        constexpr auto en_passant_hash (unsigned int file) {return file < 8 ? en_passant_randoms[file] : 0;}
}

constexpr
auto zobrist_hash(const Position &pos) -> uint64_t;

struct PositionHashPair {

        PositionHashPair() = default;
        PositionHashPair (const Position &position, uint64_t zhash)
                : pos(position),
                  hash(zhash)
        { }

        Position pos;
        uint64_t hash;  // associated hash pair;
};



constexpr
auto zobrist_hash(const Position &pos) -> uint64_t
{
        using namespace HashConstants;

        uint64_t hash = 0ull;
        // we just xor all the properties

        // all pieces (64 squares * 12 pieces)
        // technically we can have less for the pawns. Too bad!

        for (int piece = 0; piece < 12; piece++) {
                const auto pc = static_cast<Epiece>(piece);
                Field f = pos.piece_field(pc);
                for (int sq = 0; sq < 64; sq++) {
                        if (f & square_from_shift(sq)) {
                                // there is a Piece of value piece at Square sq
                                hash ^= piece_square_hash(pc, sq);
                        }
                }
        }

        if (pos.meta.active == Color::black)
                hash ^= black_move_hash;

        hash ^= castling_right_hash(pos.meta.castle_rights);

        hash ^= en_passant_hash(pos.meta.pawn2fwd_file());

        return hash;
}

#endif //ZOBRIN_HASH_H
