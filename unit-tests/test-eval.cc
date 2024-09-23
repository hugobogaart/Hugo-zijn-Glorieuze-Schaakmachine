//
// Created by Hugo Bogaart on 23/07/2024.
//

#include "../src/cli/cli-utils.h"
#include "../src/cli/cli-game.h"

#include "../src/Engine/position.h"
#include "../src/Engine/eval.h"

auto test_eval () -> void;

#include <iostream>

auto test_eval_deep () -> void
{
        /*
        double time;
        float eval;
        {
                Timer<double, std::chrono::seconds> _(time);
                eval = eval_deep(start_position, 6);
        }
        std::cout << "start eval " << eval << std::endl;
        std::cout << "time " << time << std::endl;
        */

        /*
        Res res;
        {
                Timer<double, std::chrono::seconds> _(time);
                res = alphabeta(start_position, 7);
        }

        std::cout << "start alphabeta eval " << res.ev << std::endl;
        std::cout << "time " << time << std::endl;

        print(maybe_make_move<Color::white>(res.best_mv, start_position).value());

        {
                Timer<double, std::chrono::seconds> _(time);
                res = alphabeta_failhard(start_position, 7);
        }

        std::cout << "start alphabeta failhard eval " << res.ev << std::endl;
        std::cout << "time " << time << std::endl;

        print(maybe_make_move<Color::white>(res.best_mv, start_position).value());
        */
}

auto test_eval () -> void
{
        test_eval_deep();
}