//
// Created by Hugo Bogaart on 15/08/2024.
//
// this file contains the lookup tables for the bitfield functions
// regardless of wether or not they are used
//

#include "bitfield.h"

constexpr std::array<Field, 64 * 8> free_ray_lookup_table = {
#include "../../tables/free_ray_lookup_table.txt"
};

constexpr std::array<Field, 64 * 64 * 8> weakly_blocked_ray_lookup_table = {
#include "../../tables/weakly_blocked_ray_lookup_table.txt"
};

// all diagonals and straights, unobstructed or obstructed (by strong obstacles)
constexpr std::array<Field, 64> free_diagonals_lookup_table = {
#include "../../tables/free_diagonals_lookup_table.txt"
};

constexpr std::array<Field, 64 * max_w_diagonal_configs> weakly_blocked_diagonals_lookup_table = {
#include "../../tables/weakly_blocked_diagonals_lookup_table.txt"
};

constexpr std::array<Field, 64> free_straights_lookup_table = {
#include "../../tables/free_straights_lookup_table.txt"
};

constexpr std::array<Field, 64 * max_w_straight_configs> weakly_blocked_straights_lookup_table = {
#include "../../tables/weakly_blocked_straights_lookup_table.txt"
};

