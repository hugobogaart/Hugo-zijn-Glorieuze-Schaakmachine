//
// Created by Hugo Bogaart on 28/07/2024.
//
#include "../src/Engine/transtable.h"
#include "../src/Engine/engine.h"
#include <iostream>

auto test_transtable () -> void;

auto print_numbers ()
{
        auto print_range = []<class Range>(const Range &range) {
                for (const auto &line : range) {
                        std::cout << line << '\n';
                }
        };

        using namespace HashConstants;
        print_range(piece_square_randoms);
        std::cout << black_move_hash << '\n';
        print_range(castling_right_randoms);
        print_range(en_passant_randoms);
}

auto test_bucket () -> void
{
        TransTable::Bucket bucky;
        auto p1 = bucky.find_empty();
        p1->hash = 1;
        auto p2 = bucky.find_empty();
        p2->hash = 2;
        auto p3 = bucky.find_empty();
        p3->hash = 3;
        auto p4 = bucky.find_empty();
        p4->hash = 4;
        auto p5 = bucky.find_empty();

        assert (p1 == bucky.data().begin());
        assert (p2 == p1 + 1);
        assert (p3 == p2 + 1);
        assert (p4 == p3 + 1);
        assert (p5 == nullptr);

        p3->hash = 0;

        auto p6 = bucky.find_empty();
        assert (p6 == p3);
        auto p7 = bucky.contains(2);
        assert (p7 == p2);

}

auto test_resize () -> void
{
        TransTable tt(TransTable::MegaByte(1));
        tt.resize(TransTable::MegaByte(20));
        assert (tt.calculate_num_full() == 0);

        constexpr int max_depth = 5;
        const Position &pos = start_position;
        // Engine engine(pos, gigabyte_tt * 2);
        Engine engine(pos, TransTable::MegaByte(2));

        engine.iterative_deepen(1, max_depth);

        size_t num = engine.tt.calculate_num_full();
        engine.tt.resize(TransTable::MegaByte(50));
        size_t num2 = engine.tt.calculate_num_full();
        assert (num2 == num);
}

auto test_transtable() -> void
{
        // print_numbers();
        // test_bucket();
        test_resize();
}
