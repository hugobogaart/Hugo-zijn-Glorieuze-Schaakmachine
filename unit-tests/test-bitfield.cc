//
// Created by Hugo Bogaart on 21/07/2024.
//

#include "../src/Engine/bitfield.h"
#include <cassert>
#include "../src/cli/cli-utils.h"

auto test_bitfield () -> void;


auto test_field_to_file () -> void
{
        for (int rank = 0; rank < 8; rank++) {
                for (int file = 0; file < 8; file++) {
                        assert(file == square_file(OneSquare(rank, file)));
                }
        }
}

auto test_field_to_rank () -> void
{
        for (int rank = 0; rank < 8; rank++) {
                for (int file = 0; file < 8; file++) {
                        assert(rank == square_rank(OneSquare(rank, file)));
                }
        }
}

#include <thread>
#include <iostream>
#include <vector>

auto test_squares () -> void
{
        Field f = 1ull;
        for (auto &sq : all_squares) {
                assert (sq == f);
                f <<= 1;
        }

        f = 0;
        for (auto sq : all_squares)
                f ^= sq;

        assert (f == ~0ull);
}


auto show_masks () -> void
{
        for (Field f : msk::file) {
                std::cout << num_like_board(f) << "\n\n";
        }

        for (Field f : msk::rank) {
                std::cout << num_like_board(f) << "\n\n";
        }

        std::cout << num_like_board(msk::RIGHT) << "\n\n";
        std::cout << num_like_board(msk::LEFT) << "\n\n";
        std::cout << num_like_board(msk::TOP) << "\n\n";
        std::cout << num_like_board(msk::BOTTOM) << "\n\n";
        std::cout << num_like_board(msk::RIGHT2) << "\n\n";
        std::cout << num_like_board(msk::LEFT2) << "\n\n";
        std::cout << num_like_board(msk::TOP2) << "\n\n";
        std::cout << num_like_board(msk::BOTTOM2) << "\n\n";
        std::cout << num_like_board(msk::white_squares) << "\n\n";
        std::cout << num_like_board(msk::black_squares) << "\n\n";

        std::cout << "startboard\n";
        for (Field f : start_board) {
                std::cout << num_like_board(f) << "\n\n";
        }
        std::cout << "castling rooks\n\n";
        std::cout << num_like_board(white_queen_rook) << "\n\n";
        std::cout << num_like_board(white_queen_rook_to) << "\n\n";
        std::cout << num_like_board(white_king_rook) << "\n\n";
        std::cout << num_like_board(white_king_rook_to) << "\n\n";
        std::cout << num_like_board(black_queen_rook) << "\n\n";
        std::cout << num_like_board(black_queen_rook_to) << "\n\n";
        std::cout << num_like_board(black_king_rook) << "\n\n";
        std::cout << num_like_board(black_king_rook_to) << "\n\n";

        std::cout << "king castling\n";
        std::cout << num_like_board(white_king_start) << "\n\n";
        std::cout << num_like_board(white_king_queenside_to) << "\n\n";
        std::cout << num_like_board(white_king_kingside_to) << "\n\n";
        std::cout << num_like_board(black_king_start) << "\n\n";
        std::cout << num_like_board(black_king_queenside_to) << "\n\n";
        std::cout << num_like_board(black_king_kingside_to) << "\n\n";

        std::cout << "castling freezones\n";
        std::cout << num_like_board(white_castle_queen_freezone) << "\n\n";
        std::cout << num_like_board(white_castle_king_freezone) << "\n\n";
        std::cout << num_like_board(black_castle_queen_freezone) << "\n\n";
        std::cout << num_like_board(black_castle_king_freezone) << "\n\n";

        std::cout << "castling safezones\n";
        std::cout << num_like_board(white_castle_queen_safezone) << "\n\n";
        std::cout << num_like_board(white_castle_king_safezone) << "\n\n";
        std::cout << num_like_board(black_castle_queen_safezone) << "\n\n";
        std::cout << num_like_board(black_castle_king_safezone) << "\n\n";

}

auto test_directions () -> void
{
        assert (north == flip_dir(south));
        assert (south == flip_dir(north));
        assert (east == flip_dir(west));
        assert (west == flip_dir(east));
        assert (northEast == flip_dir(southWest));
        assert (southWest == flip_dir(northEast));
        assert (southEast == flip_dir(northWest));
        assert (northWest == flip_dir(southEast));
}

// tests if pdep and pext are each others inverses
auto test_bit_twiddlies () -> void
{
        uint64_t seed = 3348105;
        auto next_random = [&]() -> uint64_t {
                seed ^= seed >> 12;
                seed ^= seed << 23;
                seed ^= seed >> 27;
                return (seed ^ seed << 33) & ~1ull;
        };

        for (int i = 0; i < 100000; ++i) {
                uint64_t mask = next_random();
                uint64_t input = next_random();


                assert((input & mask) == pdep(pext(input, mask), mask));
        }
}

auto test_rays () -> void {
        OneSquare sq(1, 3);
        Field weak = OneSquare(5, 3);
        // Field weak = 0;
        Field strong = OneSquare(5, 3);

        const Field strong_obstacles = strong | shifted<north>(weak);
        const Field ray = get_free_ray<north>(sq);
}

auto test_tables () -> void
{
        ;
}


auto test_bitfield() -> void
{
        // test_squares();
        // test_field_to_file();
        // test_field_to_rank();
        // show_masks();
        // test_squares();
        // test_directions();
        // test_bit_twiddlies();
        test_rays();
        test_tables();
}
