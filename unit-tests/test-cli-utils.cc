//
// Created by Hugo Bogaart on 18/07/2024.
//

#include "../src/cli/cli-utils.h"
#include "../src/Engine/movegen.h"
#include "../src/Engine/position.h"
#include <cassert>
#include <iostream>

auto test_cli_utils () -> void;

auto test_str_to_uint () -> void
{
        // test values that should work
        auto testval = [](const std::string &str, uint64_t res) {
                auto opt = str_to_uint(str);
                assert (opt.has_value() && opt.value() == res);
        };

        testval("0", 0);
        testval("122", 122);
        testval("123555678", 123555678);
        testval("10", 10);

        // test values that should not work
        auto testneg = [](const std::string &str) {
                auto opt = str_to_uint(str);
                assert (opt.has_value() == false);
        };

        testneg("");
        testneg("123 4");
        testneg(" 1223");
        testneg("00000000000 000");
}

auto test_to_words () -> void
{
        typedef std::vector<std::string> strvec;

        assert (to_words("").empty());
        assert (to_words("hallo") == strvec({"hallo"}));
        assert (to_words("hallo     hallo") == strvec({"hallo", "hallo"}));
        assert (to_words("     v                        ") == strvec({"v"}));
        assert (to_words("a a a a a a aaa aa a bhsgds ahe") == strvec({"a", "a","a", "a", "a", "a", "aaa", "aa","a", "bhsgds", "ahe"}));
        assert (to_words("   \t\t\t     \n      ").empty());
}


auto test_fromFen () -> void
{
        /*
         * start board:
         * "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
         *
         * after 1.e4
         * "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"
         *
         * then after 1...c5:
         * "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2"
         *
         * then after 2.Nf3:
         * "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2"
         */


        typedef std::optional<Position> OptB;
        typedef std::optional<Move> OptMv;

        const OptB f0 = fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        const OptB f1 = fromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
        const OptB f2 = fromFen("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");
        const OptB f3 = fromFen("rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");

        //static_assert(f0);
        //static_assert(f1);
        //static_assert(f2);
        //static_assert(f3);


        Position board0, board1, board2, board3;

        board0 = start_position;
        OptMv m1 = fromAlgebraic("e2e4", board0);
        assert(m1);

        board1 = maybe_make_move<Color::white>(*m1, board0).value();

        OptMv m2 = fromAlgebraic("c7c5", board1);
        assert (m2);
        board2 = maybe_make_move<Color::black>(*m2, board1).value();

        OptMv m3 = fromAlgebraic("g1f3", board2);
        assert(m3);
        board3 = maybe_make_move<Color::white>(*m3, board2).value();


        // MetaData mf = f1->meta, mb = board1.meta;
        assert (*f0 == board0);
        assert (*f1 == board1);
        assert (*f2 == board2);
        assert (*f3 == board3);
}


auto test_fromAlgebraic () -> void
{
        // things like en passant and castling are handled as special cases in CompactMoves
        // therefore there must be a board to interpret these moves with

        constexpr Position pos = start_position;


        std::optional<Move> mv1 = fromAlgebraic("e2e4", pos);
        assert (mv1.has_value());
        assert (mv1->get_special() == Move::none);
        assert (mv1->from_file() == 4);
        assert (mv1->from_rank() == 1);
        assert (mv1->to_file() == 4);
        assert (mv1->to_rank() == 3);

        std::optional<Move> mv2 = fromAlgebraic("e7e5", pos);
        assert (mv2.has_value());
        assert (mv2->get_special() == Move::none);
        assert (mv2->from_file() == 4);
        assert (mv2->from_rank() == 6);
        assert (mv2->to_file() == 4);
        assert (mv2->to_rank() == 4);

        std::optional<Move> mv3 = fromAlgebraic("e1g1", pos);
        assert (mv3.has_value());
        assert (mv3->get_special() == Move::Special::castle);
        assert (mv3->get_castle_type() == Move::CastleType::kingside);

        std::optional<Move> mv4 = fromAlgebraic("e7e8q", pos);
        assert (mv4.has_value());
        assert (mv4->get_special() == Move::promotion);
        assert (mv4->from_file() == 4);
        assert (mv4->from_rank() == 6);
        assert (mv4->to_file() == 4);
        assert (mv4->to_rank() == 7);
        assert (mv4->get_promotion() == Move::queen_promo);

        std::optional<Move> mv5 = fromAlgebraic("e7e8n", pos);
        assert (mv5.has_value());
        assert (mv5->get_special() == Move::Special::promotion);
        assert (mv5->from_file() == 4);
        assert (mv5->from_rank() == 6);
        assert (mv5->to_file() == 4);
        assert (mv5->to_rank() == 7);
        assert (mv5->get_promotion() == Move::horse_promo);


        assert(fromAlgebraic("e7e8k", pos).has_value() == false);
        assert(fromAlgebraic("a2j8", pos).has_value() == false);
        assert(fromAlgebraic("a103", pos).has_value() == false);
        assert(fromAlgebraic("17e8", pos).has_value() == false);
}

auto test_toAlgebraic () -> void
{
        constexpr Position pos = start_position;

        std::string test_str1 = "e7e8n";
        auto res = toAlgebraic(*fromAlgebraic(test_str1, pos), pos.meta.active);
        assert (toAlgebraic(*fromAlgebraic(test_str1, pos), pos.meta.active) == test_str1);
}

auto test_print() -> void
{
        std::cout << boards2str(start_position, start_position);
}

auto test_cli_utils() -> void
{
        test_str_to_uint();
        test_to_words();
        test_fromFen();
        test_fromAlgebraic();
        test_toAlgebraic();
        test_print();
}
