//
// Created by Hugo Bogaart on 18/07/2024.
//

#include "unit-tests.h"

// the main tests from all unit test files
extern auto test_bitfield       () -> void;
extern auto test_eval           () -> void;
extern auto test_position       () -> void;
extern auto test_cli_utils      () -> void;
extern auto test_uci            () -> void;
extern auto test_movegen        () -> void;
extern auto test_transtable     () -> void;
extern auto test_engine         () -> void;

auto run_tests () -> void
{

        // test_bitfield();
        // test_eval();
        // test_position();
        // test_cli_utils();
        // test_uci();
        test_movegen();
        // test_transtable();
        // test_engine();
}
