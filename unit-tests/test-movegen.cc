//
// Created by Hugo Bogaart on 25/07/2024.
//

#include "../src/Engine/movegen.h"
#include "../src/Engine/position.h"
#include <algorithm>
#include <iostream>
#include <cassert>
#include "../src/Engine/zobrist-hash.h"
#include "../src/cli/cli-utils.h"

auto test_movegen () -> void;

auto test_generate_moves () -> void
{
        auto begin_board = start_position;
        MoveList lst;
        generate_moves<Color::white>(begin_board, lst);
        assert(lst.size() == 20);
}

#include "../src/cli/cli-game.h"

template <Color col>
auto perft_col (const Position &position, int ply) -> size_t
{
        if (ply == 0)
                return 1;

        MoveList mlist;
        //generate_moves2<col>(position, mlist);
        generate_moves_sorted<col>(position, mlist);

        size_t count = 0;
        for (Move mv : mlist) {
                Position copy = position;
                make_move_unsafe<col>(mv, copy);

                /*
                auto k = maybe_make_move<col>(mv, position);
                if (!k) {
                        print(position);
                        print(copy);
                        std::cout << (int)mv.from_file() << '\t' << (int)mv.from_rank() << "\n"
                        << (int)mv.to_file() << '\t' << (int)mv.to_rank() << "\n" << mv.special << std::endl;
                }
                */

                count += perft_col<!col>(copy, ply - 1);
        }
        return count;
}


auto perft (const Position &position, int ply) -> size_t
{
        if (position.meta.active == Color::white)
                return perft_col<Color::white>(position, ply);
        return perft_col<Color::black>(position, ply);
}

template <Color col>
auto perft_compare_col (const Position &position, int ply) -> size_t
{
        if (ply == 0)
                return 1;

        MoveList mlist, mlist2;
        generate_moves<col>(position, mlist);
        generate_moves2<col>(position, mlist2);

        size_t count = 0;

        // check if all moves have been generated
        for (Move mv1 : mlist) {
                auto pred = [&](const Move mv) -> bool {return mv == mv1;};
                if (std::none_of(mlist2.begin(), mlist2.end(), pred)) {
                        Position copy = position;
                        make_move_unsafe<col>(mv1, copy);
                        std::cout << "Move that failed to be generated:\n";
                        std::cout << boards2str(position, copy) << std::endl;
                        generate_moves2<col>(position, mlist2);
                        std::abort();
                }
        }

        // check no other moves were generated
        for (Move mv2 : mlist2) {
                auto pred = [&](const Move mv) -> bool {return mv == mv2;};
                if (std::none_of(mlist.begin(), mlist.end(), pred)) {
                        Position copy = position;
                        make_move_unsafe<col>(mv2, copy);
                        std::cout << "Move that was erroneously generated:\n";
                        std::cout << boards2str(position, copy) << std::endl;
                        generate_moves2<col>(position, mlist2);
                        std::abort();
                }
        }

        // check if there are duplicates
        for (int i = 0; i < mlist2.size(); ++i) {
                Move mv2 = mlist2[i];
                auto dupe = [&](const Move mv) -> bool { return mv == mv2;};
                if (std::any_of(mlist2.begin(), mlist2.begin() + i, dupe)) {
                        Position copy = position;
                        make_move_unsafe<col>(mv2, copy);
                        std::cout << "Duplicate generated:\n";
                        std::cout << boards2str(position, copy) << std::endl;
                        generate_moves2<col>(position, mlist2);
                        std::abort();
                }
        }

        for (Move mv : mlist) {

                Position copy = position;
                make_move_unsafe<col>(mv, copy);
                count += perft_compare_col<!col>(copy, ply - 1);
        }
        return count;
}

auto perft_compare (const Position &position, int ply) -> void
{
        if (position.meta.active == Color::white)
                perft_compare_col<Color::white>(position, ply);
        else
                perft_compare_col<Color::black>(position, ply);
}

auto test_perft () -> void
{
        // tests the movegen on a position up to a maximum ply, given some results
        auto test_pos = []<size_t n>(const Position &pos, int max_ply, const std::array<size_t, n> &results) {
                for (int ply = 1; ply <= max_ply; ply++) {
                        size_t res;
                        double time;
                        {
                                Timer<double, std::chrono::seconds> _(time);
                                res = perft(pos, ply);
                        }
                        std::cout << "\tdepth " << ply << '\t' << res << '\n';
                        // std::cout << "\tthis took " << time << " s" << std::endl;
                        if (res != results[ply - 1]) {
                                std::cout << "Error!\t the real number should be " << results[ply - 1] << std::endl;
                        }
                }
        };

        // the perft function returns the amount of leaf nodes visited where the game is not finished
        // good way to test movegen is to verify perft results
        // https://www.chessprogramming.org/Perft_Results

        std::cout << "perft test\n";

        Position pos1 = start_position;
        std::array<size_t, 6> pos1_results = {
                20,
                400,
                8902,
                197281,
                4865609,
                119060324
        };
        std::cout << "\nposition 1\n";
        test_pos(pos1, 6, pos1_results);


        const std::optional<Position> pos2_ = fromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
        // static_assert(pos2_);
        const auto pos2= *pos2_;
        std::array<size_t, 5> pos2_results = {
                48,
                2039,
                97862,
                4085603,
                193690690
        };
        std::cout << "\nposition 2\n";
        test_pos(pos2, 4, pos2_results);


        const std::optional<Position> pos3_ = fromFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
        //static_assert(pos3_);
        const auto pos3= *pos3_;
        std::array<size_t, 8> pos3_results = {
                14,
                191,
                2812,
                43238,
                674624,
                11030083,
                178633661,
                3009794393
        };
        std::cout << "\nposition 3\n";
        test_pos(pos3, 6, pos3_results);


        const std::optional<Position> pos4_ = fromFen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
        //static_assert(pos4_);
        const auto pos4= *pos4_;
        std::array<size_t, 6> pos4_results = {
                6,
                264,
                9467,
                422333,
                15833292,
                706045033
        };
        std::cout << "\nposition 4\n";
        test_pos(pos4, 5, pos4_results);


        const std::optional<Position> pos5_ = fromFen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
        //static_assert(pos5_);
        const auto pos5= *pos5_;
        std::array<size_t, 5> pos5_results = {
                44,
                1486,
                62379,
                2103487,
                89941194
        };
        std::cout << "\nposition 5\n";
        test_pos(pos5, 5, pos5_results);


        const std::optional<Position> pos6_ = fromFen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
        //static_assert(pos6_);
        const auto pos6= *pos6_;
        std::array<size_t, 9> pos6_results = {
                46,
                2079,
                89890,
                3894594,
                164075551,
                6923051137,
                287188994746,
                11923589843526,
                490154852788714
        };
        std::cout << "\nposition 6\n";
        test_pos(pos6, 5, pos6_results);
}

size_t hash_misses = 0;

template <Color col>
auto perft_col2 (const PositionHashPair &pos_hash, int ply) -> size_t
{
        if (ply == 0)
                return 1;

        MoveList mlist;
        generate_moves<col>(pos_hash.pos, mlist);

        size_t count = 0;
        for (Move mv : mlist) {
                PositionHashPair copy = pos_hash;
                make_move_unsafe<col>(mv, copy);
                if (copy.hash != zobrist_hash(copy.pos)) {
/*
                        std::cout << "/\n";
                        print(pos_hash.pos);
                        print(copy.pos);
                        auto spec = mv.get_special();
                        auto promo = mv.get_promotion();
                        auto castle = mv.get_castle_type();
*/
                        hash_misses++;
                }
                count += perft_col2<!col>(copy, ply - 1);
        }
        return count;
}


auto perft2 (const PositionHashPair &pos_hash, int ply) -> size_t
{
        if (pos_hash.pos.meta.active == Color::white)
                return perft_col2<Color::white>(pos_hash, ply);
        return perft_col2<Color::black>(pos_hash, ply);
}

// the other movegen function
auto test_perft2 () -> void
{
        // tests the movegen on a position up to a maximum ply, given some results
        auto test_pos = []<size_t n>(const Position &pos, int max_ply, const std::array<size_t, n> &results) {
                PositionHashPair pos_hash{};
                pos_hash.pos = pos;
                pos_hash.hash = zobrist_hash(pos);
                for (int ply = 1; ply <= max_ply; ply++) {
                        size_t res;
                        double time;
                        {
                                Timer<double, std::chrono::seconds> _(time);
                                res = perft2(pos_hash, ply);
                        }
                        std::cout << "\tdepth " << ply << '\t' << res << '\n';
                        // std::cout << "\tthis took " << time << " s" << std::endl;
                        if (res != results[ply - 1]) {
                                std::cout << "Error!\t the real number should be " << results[ply - 1] << std::endl;
                        }
                }
        };

        // the perft function returns the amount of leaf nodes visited where the game is not finished
        // good way to test movegen is to verify perft results
        // https://www.chessprogramming.org/Perft_Results

        std::cout << "perft test\n";

        Position pos1 = start_position;
        std::array<size_t, 6> pos1_results = {
                20,
                400,
                8902,
                197281,
                4865609,
                119060324
        };
        std::cout << "\nposition 1\n";
        test_pos(pos1, 6, pos1_results);


        const std::optional<Position> pos2_ = fromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
        //static_assert(pos2_);
        const auto pos2= *pos2_;
        std::array<size_t, 5> pos2_results = {
                48,
                2039,
                97862,
                4085603,
                193690690
        };
        std::cout << "\nposition 2\n";
        test_pos(pos2, 4, pos2_results);


        const std::optional<Position> pos3_ = fromFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
        //static_assert(pos3_);
        const auto pos3= *pos3_;
        std::array<size_t, 8> pos3_results = {
                14,
                191,
                2812,
                43238,
                674624,
                11030083,
                178633661,
                3009794393
        };
        std::cout << "\nposition 3\n";
        test_pos(pos3, 6, pos3_results);


        const std::optional<Position> pos4_ = fromFen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
        //static_assert(pos4_);
        const auto pos4= *pos4_;
        std::array<size_t, 6> pos4_results = {
                6,
                264,
                9467,
                422333,
                15833292,
                706045033
        };
        std::cout << "\nposition 4\n";
        test_pos(pos4, 5, pos4_results);


        const std::optional<Position> pos5_ = fromFen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
        //static_assert(pos5_);
        const auto pos5= *pos5_;
        std::array<size_t, 5> pos5_results = {
                44,
                1486,
                62379,
                2103487,
                89941194
        };
        std::cout << "\nposition 5\n";
        test_pos(pos5, 5, pos5_results);


        const std::optional<Position> pos6_ = fromFen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
        //static_assert(pos6_);
        const auto pos6= *pos6_;
        std::array<size_t, 9> pos6_results = {
                46,
                2079,
                89890,
                3894594,
                164075551,
                6923051137,
                287188994746,
                11923589843526,
                490154852788714
        };
        std::cout << "\nposition 6\n";
        test_pos(pos6, 5, pos6_results);

        std::cout << "\nhash misses: " << hash_misses << std::endl;
}

auto benchmark_movegen ()
{
        double time;
        size_t num;
        {
                Timer<double, std::chrono::seconds> _(time);
                num = perft(start_position, 5);
        }
        std::cout << "perft took " << time << " seconds\nto evaluate " << num << " positions." << std::endl;
}



auto test_movegen() -> void
{
        // test_generate_moves();
        // benchmark_movegen();

        test_perft();
        // test_perft2(); // also tests hash propagation

        const std::optional<Position> pos6_ = fromFen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
        //static_assert(pos6_);
        const auto test_pos= *pos6_;
        //perft_compare(test_pos, 5);
}
