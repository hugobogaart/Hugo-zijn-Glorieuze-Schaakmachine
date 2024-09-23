//
// Created by Hugo Bogaart on 16/08/2024.
//

#include <filesystem>
#include <fstream>
#include <memory>
#include <iostream>

// this makes the header compile without tables present
#define ALL_CALC
#include "../src/Engine//bitfield.h"

struct TableFuncs {

        /*
        template <Direction dir>
        static auto make_free_ray_lookup_table () -> std::unique_ptr<std::array<Field, 64>>
        {
                auto p = std::make_unique<std::array<Field, 64>>();
                std::array<Field, 64> &arr = *p;
                for (OneSquare sq : all_squares)
                        arr[square_to_shift(sq)] = get_free_ray_calc<dir>(sq);
                return p;
        }
        */
        static auto make_free_ray_lookup_table () -> std::unique_ptr<std::array<Field, 64 * 8>>
        {
                auto p = std::make_unique<std::array<Field, 64 * 8>>();
                std::array<Field, 64 * 8> &arr = *p;

                // shenanigans because we do all the diections in one array
                auto gen_part = [&]<Direction dir>() -> void {
                        for (OneSquare sq : all_squares) {
                                arr[64 * dir + square_to_shift(sq)] = get_free_ray_calc<dir>(sq);
                        }
                };

                // stupid fucking c++ templated lambda's
                gen_part.template operator()<north>();
                gen_part.template operator()<east>();
                gen_part.template operator()<south>();
                gen_part.template operator()<west>();
                gen_part.template operator()<northEast>();
                gen_part.template operator()<northWest>();
                gen_part.template operator()<southEast>();
                gen_part.template operator()<southWest>();

                return p;
        }

        static auto make_weakly_blocked_ray_lookup_table () -> std::unique_ptr<std::array<Field, 64 * 64 * 8>>
        {
                auto p = std::make_unique<std::array<Field, 64 * 64 * 8>>();
                std::array<Field, 64 * 64 * 8> &arr = *p;

                // shenanigans because we do all the diections in one array
                auto gen_part = [&]<Direction dir>() -> void {
                        for (int shift = 0; shift < 64; ++shift) {
                                OneSquare sq = square_from_shift(shift);
                                const int file = shift_to_file(shift);
                                const int rank = shift_to_rank(shift);
                                Field relevant_bits = get_free_ray_calc<dir>(sq);
                                if (file != 0)
                                        relevant_bits &= ~msk::file[0];
                                if (file != 7)
                                        relevant_bits &= ~msk::file[7];
                                if (rank != 0)
                                        relevant_bits &= ~msk::rank[0];
                                if (rank != 7)
                                        relevant_bits &= ~msk::rank[7];

                                for (uint64_t config_idx = 0; config_idx < 64; ++config_idx) {
                                        const uint64_t blockers = pdep(config_idx, relevant_bits);
                                        arr[dir * (64 * 64) + shift * 64 + config_idx] = get_weakly_blocked_ray_calc<dir>(sq, blockers);
                                }
                        }
                };

                // stupid fucking c++ templated lambda's
                gen_part.template operator()<north>();
                gen_part.template operator()<east>();
                gen_part.template operator()<south>();
                gen_part.template operator()<west>();
                gen_part.template operator()<northEast>();
                gen_part.template operator()<northWest>();
                gen_part.template operator()<southEast>();
                gen_part.template operator()<southWest>();

                return p;
        }

        static auto make_free_diagonals_lookup_table () -> std::unique_ptr<std::array<Field, 64>>
        {
                auto p = std::make_unique<std::array<Field, 64>>();
                std::array<Field, 64> &arr = *p;
                for (OneSquare sq : all_squares)
                        arr[square_to_shift(sq)] = get_free_diagonals_calc(sq);
                return p;
        }

        static auto make_weakly_blocked_diagonals_lookup_table () -> std::unique_ptr<std::array<Field, 64 * max_w_diagonal_configs>>
        {
                auto p = std::make_unique<std::array<Field, 64 * max_w_diagonal_configs>>();
                std::array<Field, 64 * max_w_diagonal_configs> &arr = *p;
                for (int shift = 0; shift < 64; ++shift) {
                        const OneSquare sq = square_from_shift(shift);
                        const Field relevant_bits = get_free_diagonals_calc(sq) & ~msk::edges;
                        for (uint64_t config_idx = 0; config_idx < max_w_diagonal_configs; ++config_idx) {
                                const uint64_t blockers = pdep(config_idx, relevant_bits);
                                arr[shift * max_w_diagonal_configs + config_idx] = get_weakly_blocked_diagonals_calc(sq, blockers);
                        }
                }
                return p;
        }

        static auto make_free_straights_lookup_table () -> std::unique_ptr<std::array<Field, 64>>
        {
                auto p = std::make_unique<std::array<Field, 64>>();
                std::array<Field, 64> &arr = *p;
                for (OneSquare sq : all_squares)
                        arr[square_to_shift(sq)] = get_free_straights_calc(sq);
                return p;
        }

        static auto make_weakly_blocked_straights_lookup_table () -> std::unique_ptr<std::array<Field, 64 * max_w_straight_configs>>
        {
                auto p = std::make_unique<std::array<Field, 64 * max_w_straight_configs>>();
                std::array<Field, 64 * max_w_straight_configs> &arr = *p;
                for (int shift = 0; shift < 64; ++shift) {
                        const OneSquare sq = square_from_shift(shift);
                        Field relevant_bits = get_free_straights_calc(sq);
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

                        for (uint64_t config_idx = 0; config_idx < max_w_straight_configs; ++config_idx) {
                                const uint64_t blockers = pdep(config_idx, relevant_bits);
                                arr[shift * max_w_straight_configs + config_idx] = get_weakly_blocked_straights_calc(sq, blockers);
                        }
                }
                return p;
        }
};

template <size_t N>
auto table_to_file (const std::array<Field, N> &array, const std::filesystem::path &path) -> void
{
        std::ofstream file(path, std::ios::out | std::ios::trunc);
        for (size_t i = 0; i < N - 1; i++) {
                file << "0x" << std::hex << array[i] << "ull,\n";
        }
        file << "0x" << std::hex << array[N - 1] << "ull\n";
        file.close();
}

auto generate_files () -> void
{
        // checking we are in the right path
        std::string pwd = std::filesystem::current_path().string();
        // we check that we are indeed in the "tables/" directory
        if (!pwd.ends_with("/tables")) {
                std::cerr << "we are not in the right directory! aborting\n";
                return;
        }

        // table gen
        const auto free_ray_lookup_table= TableFuncs::make_free_ray_lookup_table();
        const auto weakly_blocked_ray_lookup_table = TableFuncs::make_weakly_blocked_ray_lookup_table();

        const auto free_diagonals_lookup_table = TableFuncs::make_free_diagonals_lookup_table();
        const auto weakly_blocked_diagonals_lookup_table = TableFuncs::make_weakly_blocked_diagonals_lookup_table();
        const auto free_straights_lookup_table = TableFuncs::make_free_straights_lookup_table();
        const auto weakly_blocked_straights_lookup_table = TableFuncs::make_weakly_blocked_straights_lookup_table();

        table_to_file(*free_ray_lookup_table, "free_ray_lookup_table.txt");
        table_to_file(*weakly_blocked_ray_lookup_table, "weakly_blocked_ray_lookup_table.txt");

        table_to_file(*free_diagonals_lookup_table, "free_diagonals_lookup_table.txt");
        table_to_file(*weakly_blocked_diagonals_lookup_table, "weakly_blocked_diagonals_lookup_table.txt");
        table_to_file(*free_straights_lookup_table, "free_straights_lookup_table.txt");
        table_to_file(*weakly_blocked_straights_lookup_table, "weakly_blocked_straights_lookup_table.txt");
}

int main (int argc, char **argv)
{
        generate_files();
}
