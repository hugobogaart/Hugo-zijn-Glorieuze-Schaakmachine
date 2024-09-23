//
// Created by Hugo Bogaart on 28/07/2024.
//

#include "../src/Engine/engine.h"
#include <chrono>
#include "../src/cli/cli-utils.h"
#include "../src/cli/cli-game.h"
#include <iostream>
#include "../src/Engine/movegen.h"
auto test_engine () -> void;


auto test_nodegen() -> void
{
        constexpr int deep = 8;

        const std::optional<Position> pos5_ = fromFen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
        //static_assert(pos5_);
        // constexpr auto pos= *pos5_;

        constexpr auto pos = start_position;
        constexpr Color first_col = pos.meta.active;

        Engine engine(pos, TransTable::MegaByte(2048));

        double time_table;
        {
                Timer<double, std::chrono::seconds> _(time_table);
                // engine.fill_alpha_beta(deep);
                engine.iterative_deepen(1, deep);
        }

        Eval ev = engine.demand_eval().value();
        // Move mv = engine.demand_best_move().value();

        std::cout << time_table << " seconds\n" << ev << std::endl;
        std::cout << engine.total_nodes_searched << " positions calculated\n";

        Position position = pos;
        uint64_t hash     = zobrist_hash(pos);
        PositionHashPair ph(pos, hash);

        print(position);

        auto p = engine.tt.find(hash);
        Color col = first_col;
        while (p->depth_searched != 0) {
                // best_move *should* be well-defined
                Move best_mv = p->best_move;

                if (col == Color::white) {
                        make_move_unsafe<Color::white>(best_mv, position);
                        make_move_unsafe<Color::white>(best_mv, ph);
                } else {
                        make_move_unsafe<Color::black>(best_mv, position);
                        make_move_unsafe<Color::black>(best_mv, ph);
                }

                hash = zobrist_hash(position);

                if (hash != ph.hash) {
                        std::cout << "hash error!\n" << hash << '\n' << ph.hash << std::endl;
                        print(position);
                        print(ph.pos);
                }

                print(position);

                p = engine.tt.find(hash);

                if (p == nullptr) {
                        break;
                }
                col = !col;
        }
}

auto test_threads () -> void
{
        constexpr int max_depth = 8;
        const Position &pos = start_position;
        Engine engine(pos, TransTable::MegaByte(2048));

        std::cout << "filled for " << engine.filled_permille() << " \U00002030"<< std::endl;

        engine.start_iterative_deepen(max_depth);

        constexpr int tickbuf = 6;
        auto timebuff = [&](int sec) -> std::string {
                std::string s = std::to_string(sec);
                while (s.size() < tickbuf)
                        s.push_back(' ');
                return s;
        };

        int sec = 0;
        while (true) {
                int depth = 0;
                auto p = engine.tt.find(engine.root.hash);
                if (p)
                        depth = p->depth_searched;

                std::cout << "second " << timebuff(sec) << "depth " << depth << std::endl;
                if (depth >= max_depth)
                        break;

                std::this_thread::sleep_for(std::chrono::seconds(1));
                sec++;
        }
        std::cout << "filled for " << engine.filled_permille() << " \U00002030"<< std::endl;
        /*
        engine.stop_after(std::chrono::seconds(20));
        std::this_thread::sleep_for(std::chrono::seconds(40));
        */
}


auto test_engine () -> void
{
        // test_nodegen();
        test_threads();
}