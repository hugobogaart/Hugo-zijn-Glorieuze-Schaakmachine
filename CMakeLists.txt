
# for profiling set
# -fno-omit-frame-pointer
# -mno-omit-leaf-frame-pointer

cmake_minimum_required(VERSION 3.26)
set(CMAKE_OSX_ARCHITECTURES "arm64")
project(GlorieuzeSchaakMachine CXX)
set(CMAKE_CXX_STANDARD 23)

add_compile_options(-Wall -Wextra -Wpedantic -march=native -flto)
add_link_options(-march=native -flto)

add_executable(GlorieuzeSchaakMachine src/main.cc
        src/cli/cli-game.h
        src/Engine/engine.cc
        src/Engine/engine.h
        src/Engine/position.h
        src/Engine/gen-defs.h
        src/Engine/position.cc
        src/Engine/eval.cc
        src/Engine/eval.h
        src/cli/cli-game.cc
        src/cli/UCI.cc
        src/cli/UCI.h
        src/cli/cli-utils.cc
        src/cli/cli-utils.h
        src/Engine/movegen.cc
        src/Engine/movegen.h
        src/Engine/zobrist-hash.h
        src/Engine/bitfield.h
        src/Engine/bitfield.cc
        src/Engine/allocators.h
        src/Engine/transtable.cc
        src/Engine/transtable.h

        unit-tests/test-eval.cc
        unit-tests/test-movegen.cc
        unit-tests/test-transtable.cc
        unit-tests/test-engine.cc
        unit-tests/test-bitfield.cc
        unit-tests/test-position.cc
        unit-tests/unit-tests.cc
        unit-tests/unit-tests.h
        unit-tests/test-cli-utils.cc
        unit-tests/test-uci.cc)
