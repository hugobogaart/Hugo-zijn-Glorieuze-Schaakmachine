//
// Created by Hugo Bogaart on 17/07/2024.
//

#include "UCI.h"
#include <iostream>
#include <string>
#include <vector>
#include <optional>

#include "cli-utils.h"
#include "../Engine/engine.h"
#include "../Engine/position.h"

struct UCIState {
        bool debug = false;


        std::optional<std::string> registered_name = std::nullopt;

        // screw it just keep the code a string
        std::optional<std::string> registered_code = std::nullopt;

        auto registered () const -> bool {return registered_name || registered_code;}

        TransTable::MegaByte tt_size = TransTable::MegaByte(1);

};

auto get_line() -> std::string
{
        std::string ln;
        std::getline(std::cin, ln);
        return ln;
}


// string of available options
// CHOOSE to only use names without comma, because they are stupid
constexpr auto options = "option name Hash type spin default 1 min 1 max 4096\n"
                         "option name Clear-Hash type button\n"
// no ponder yet         "option name Ponder type check\n"
                                                                        ;

struct WordParserPair;


class UCIParser {

        // it copies the vector and strings each time. Yes, this is slow. Too bad!
        const std::vector<std::string> words;
        const size_t idx = 0;

public:

        explicit
        UCIParser (const std::vector<std::string> &words_)
                : words(words_)
        { }

private:
        explicit
        UCIParser (const std::vector<std::string> &words_, size_t idx_)
                : words(words_), idx(idx_)
        { }

public:

        // finds the word and returns a parser that begins with that first word
        auto find_at (const std::string &word) const -> UCIParser;

        // finds the word and returns a parser that begins after that word
        // this may be an empty, but valid parser to show this was successful
        auto find_after (const std::string &word) const -> UCIParser;


        // idx == words.size() is still valid
        // simply an empty parser
        auto valid () const -> bool {return idx <= words.size();}

        auto empty () const -> bool {return idx >= words.size();}
        auto non_empty () const -> bool {return idx < words.size();}

        operator bool() const {return valid();}


        auto first_word () const -> std::optional<std::string>
        {
                if (non_empty())
                        return words[idx];
                return std::nullopt;
        }

        auto words_until (const std::string &str) const -> std::vector<std::string>
        {
                size_t i = idx;
                std::vector<std::string> vec;
                while (i < words.size()) {
                        const std::string &word = words[i++];
                        if (word == str)
                                return vec;
                        vec.emplace_back(word);
                }
                return vec;
        }

        auto words_until (const std::vector<std::string> &strs) const -> std::vector<std::string>
        {
                size_t i = idx;
                std::vector<std::string> vec;
                while (i < words.size()) {
                        const std::string &word = words[i++];
                        if (std::ranges::any_of(strs, [&](const auto &str) {return str == word;})) {
                                return vec;
                        }
                        vec.emplace_back(word);
                }
                return vec;
        }

        auto rest () const -> std::vector<std::string>
        {
                std::vector<std::string> vec;
                vec.reserve(words.size() - idx);
                for (size_t i = idx; i < words.size(); i++) {
                        vec.emplace_back(words[i]);
                }
                return vec;
        }


        static const UCIParser invalid_parser;
        friend auto parse_words (const UCIParser &parser, std::vector<std::string> words) -> std::optional<WordParserPair>;
};

const UCIParser UCIParser::invalid_parser = UCIParser({}, 1);


auto UCIParser::find_at (const std::string &word) const -> UCIParser
{
        size_t next_idx = idx;
        for (; next_idx < words.size(); next_idx++) {
                if (words[next_idx] == word) {
                        // hit
                        break;
                }
        }

        // if word not found, the returned parser is invalid,
        next_idx++;
        // indicating that the word has not been found
        // if the word has been found, the parser will be valid, with the rest of the
        // vector of words that come after the found words
        return UCIParser(words, next_idx);
}

auto UCIParser::find_after (const std::string &word) const -> UCIParser
{
        size_t next_idx = idx;
        for (; next_idx < words.size(); next_idx++) {
                if (words[next_idx] == word) {
                        // hit
                        break;
                }
        }
        return UCIParser(words, next_idx + 1);
}

struct WordParserPair {
        std::string word;
        UCIParser parser;
};

// returns a pair of the word from the list that was found first in the parser
// together with a parser of the rest of the strings after that word
auto parse_words (const UCIParser &parser, std::vector<std::string> words) -> std::optional<WordParserPair>
{
        if (words.empty())
                return std::nullopt;

        std::vector<UCIParser> parsers;
        parsers.reserve(words.size());
        for (const auto &word : words)
                parsers.emplace_back(parser.find_after(word));

        // now find the first succeeded parser
        const UCIParser *to_best = parsers.data();
        for (const auto &parser : parsers) {
                if (!parser)
                        continue;

                if (!*to_best || parser.idx < to_best->idx) {
                        to_best = &parser;
                }
        }

        if (!*to_best)
                return std::nullopt;

        size_t best_idx = to_best - parsers.data();
        return WordParserPair{words[best_idx], parsers[best_idx]};
}

auto merge_strings (const std::vector<std::string> &words) -> std::string
{
        std::string str;
        for (const auto &word : words) {
                if (!str.empty())
                        str.push_back(' ');
                str.append(word);
        }
        return str;
}

auto uci () -> void
{


        auto send_best_move = [] (Move mv, Color col) -> void {
                std::cout << "bestmove " << toAlgebraic(mv, col) << "\n" << std::flush;
                // no ponder
        };

        // engine is an argument so we don't capture anything
        auto send_info = [](const SendInfoArgs &args) {

                Color col = args.active_color;

                std::cout << "info ";
                if (args.depth)
                        std::cout << "depth " << *args.depth << " ";
                if (args.time_ms)
                        std::cout << "time " << *args.time_ms << " ";
                if (args.nodes)
                        std::cout << "nodes " << *args.nodes << " ";
                if (args.score) {
                        std::cout << "score ";
                        if (args.score->mate)
                                std::cout << "mate " << *args.score->mate << " ";
                        else {
                                std::cout << "cp " << args.score->engine_perspective << " ";
                                if (args.score->lower_bound)
                                        std::cout << "lowerbound ";
                                else if (args.score->upper_bound)
                                        std::cout << "upperbound ";
                        }
                }

                if (args.currmove)
                        std::cout << "currmove " << toAlgebraic(*args.currmove, col) << " ";
                if (args.hashfull)
                        std::cout << "hashfull " << *args.hashfull << " ";
                if (args.nps)
                        std::cout << "nps " << *args.nps << " ";

                // pv last
                if (args.pv) {
                        std::cout << "pv";
                        for (const Move mv : *args.pv) {
                                std::cout << ' ' << toAlgebraic(mv, col);
                        }
                }
                std::cout << "\n" << std::flush;
        };

        UCIState state;
        Engine engine(start_position, send_info, send_best_move, TransTable::MegaByte(1));

        std::cout << "id name Hugo's Glorieuze Schaakmachine\n"
                     "id author Hugo Bogaart\n";

        std::cout << options;

        std::cout << "uciok\n" << std::flush;

        // event loop
        while (true) {
                static const std::vector<std::string> commands = {
                        "debug",
                        "isready",
                        "setoption",
                        "register",
                        "ucinewgame",
                        "position",
                        "go",
                        "stop",
                        "ponderhit",
                        "quit",
                        "ping",  // custom
                        "d",    // custom "display board"
                        "mkay"
                };

                // the parser that has all words left to parse
                std::string user_input = get_line();
                UCIParser main_parser(to_words(user_input));
                auto opt_wpars = parse_words(main_parser, commands);

                if (!opt_wpars) {
                        std::cerr << "unknown command: \"" << user_input << "\"\n";
                        continue;
                }

                const std::string &word = opt_wpars->word;
                const UCIParser &parser = opt_wpars->parser;

                if (word == "debug") {
                        // search for "on" or "off"
                        static const std::vector<std::string> allowed_debug_modes = {"on", "off"};

                        auto debug_opt_wpars = parse_words(parser, allowed_debug_modes);
                        if (debug_opt_wpars) {
                                if (debug_opt_wpars->word == "on")
                                        state.debug = true;
                                else
                                        state.debug = false;
                        } else {
                                std::cerr << "invalid debug mode: \"" << merge_strings(parser.rest()) << "\"\n";
                        }

                } else if (word == "isready") {
                        std::cout << "readyok\n";
                } else if (word == "setoption") {

                        // whatever comes after "name" before "value" is the name of the option
                        // this can NOT have spaces since that is stupid, and we choose out own names

                        std::optional<std::string> option_name = parser.find_after("name").first_word();

                        if (option_name && *option_name == "Hash") {
                                /*
                                std::optional<uint64_t> new_size_mb = parser.find_after("value")
                                                                               .first_word()
                                                                               .and_then(str_to_uint);
                                */
                                std::optional<std::string> option_value = parser.find_after("value").first_word();
                                std::optional<uint64_t> new_size_mb = std::nullopt;
                                if (option_value)
                                        new_size_mb = str_to_uint(*option_value);

                                if (new_size_mb && *new_size_mb != state.tt_size.num_mbs) {
                                        engine.resize_hashtable(TransTable::MegaByte(*new_size_mb));
                                        state.tt_size = TransTable::MegaByte(*new_size_mb);
                                }
                        } // else if (option_name && *option_name == "Clear-Hash")

                        else {
                                std::cerr << "unknown option: \"" << (option_name ? *option_name : "") << "\"\n";
                        }

                } else if (word == "register") {

                        auto next_word = parser.first_word();
                        if (next_word && *next_word == "later") {
                                state.registered_name = std::nullopt;
                                state.registered_code = std::nullopt;

                        } else {
                                // there could be a name and a value
                                auto name_parser = parser.find_after("name");
                                // also stops if end of string
                                state.registered_name = merge_strings(name_parser.words_until("code"));
                                auto code_parser = parser.find_after("code");
                                state.registered_code = code_parser.first_word().value_or("");

                        }

                } else if (word == "ucinewgame") {
                        ;
                        // ignore (for now)
                        // indicates the next search (started with "position" and "go") will be from
                        // a different game

                } else if (word == "position") {
                        // next is either "startpos" or "fen <fenstring>"
                        // after that there is "moves <move1> ... <movei>"

                        std::optional<Position> pos;

                        std::vector<std::string> pos_strs = parser.words_until("moves");
                        if (pos_strs.size() == 1 && pos_strs[0] == "startpos") {
                                pos = start_position;
                                if (!pos) {
                                        std::cerr << "invalid position given " << pos_strs[0] << '\n';
                                }
                        } else if (pos_strs.size() == 2 && pos_strs[0] == "fen") {
                                pos = fromFen(pos_strs[1]);
                                if (!pos) {
                                        std::cerr << "invalid position given " << pos_strs[1] << '\n';
                                }
                        } else {
                                std::cerr << "no position given\n";
                        }

                        if (!pos) {
                                // we read a new line of input, nevermind the rest
                                continue;
                        }

                        const Position original_pos = *pos;

                        auto movesParser = parser.find_at("moves");

                        std::vector<Move> verified_moves;

                        // if "moves" is not specified, we accept that as well and use this position
                        if (movesParser) {
                                std::vector<std::string> moves_strs = movesParser.rest();

                                // for error handling, a new position instance is used
                                std::optional<Position> maybe_pos;
                                for (const std::string &move_str : moves_strs) {
                                        std::optional<Move> maybe_move = fromAlgebraic(move_str, *pos);
                                        if (!maybe_move) {
                                                std::cerr << "bad move encountered: \"" << move_str << "\"\n";
                                                pos = std::nullopt;
                                                break;
                                        }
                                        maybe_pos = maybe_make_move(*maybe_move, *pos);
                                        if (!maybe_pos) {
                                                std::cerr << "this is an illegal move: \"" << move_str << "\"\n";
                                                pos = std::nullopt;
                                                break;
                                        }
                                        pos = *maybe_pos;
                                        verified_moves.emplace_back(*maybe_move);
                                }
                        }

                        // can only fail if a bad move is encountered
                        if (pos) {
                                // engine.position(*pos);
                                engine.position(original_pos, verified_moves);
                        }

                } else if (word == "go") {

                        auto move_is_legal = [&](Move mv) -> bool {
                                return maybe_make_move(mv, engine.get_position()).has_value();
                        };

                        const static std::vector<std::string> keywords = {
                                "searchmoves",
                                "ponder",
                                "wtime",
                                "winc",
                                "btime",
                                "binc",
                                "movestogo",
                                "depth",
                                "mate",
                                "movetime",
                                "infinite"
                        };


                        Engine::GoArgs go_args;

                        // some arguments are word-only
                        // others have values

                        // searchmoves
                        auto search_moves_parser = parser.find_at("searchmoves");
                        if (search_moves_parser) {
                                // there are now a bunch of moves until another keyword
                                std::vector<std::string> algebraics = search_moves_parser.words_until(keywords);
                                MoveList moves;

                                bool all_moves_valid = true;
                                for (const auto &s : algebraics) {
                                        std::optional<Move> maybe_move = fromAlgebraic(s, engine.get_position());
                                        if (!maybe_move) {
                                                std::cerr << "invalid move: \"" << s << "\"\n";
                                                all_moves_valid = false;
                                                break;
                                        }

                                        // now we see if they are legal
                                        if (!move_is_legal(*maybe_move)) {
                                                std::cerr << "illegal move: \"" << s << "\"\n";
                                                all_moves_valid = false;
                                                break;
                                        }
                                        moves.emplace_back(*maybe_move);
                                }

                                if (!all_moves_valid) {
                                        // go failed
                                        continue;
                                }
                                go_args.move_list = moves;
                        }

                        if (parser.find_at("ponder").valid()) {
                                go_args.ponder = true;
                        }

                        auto read_arg = [&](const UCIParser &parser) -> std::optional<size_t> {
                                std::optional<std::string> fword = parser.first_word();
                                if (fword) {
                                        return str_to_uint(*fword);
                                }
                                return std::nullopt;
                        };

                        if (UCIParser wtime_parser = parser.find_at("wtime")) {
                                // go_args.wtime = wtime_parser.first_word().and_then(str_to_uint);
                                go_args.wtime = read_arg(wtime_parser);
                        }
                        if (UCIParser winc_parser = parser.find_at("winc")) {
                                // go_args.winc = winc_parser.first_word().and_then(str_to_uint);
                                go_args.winc = read_arg(winc_parser);
                        }
                        if (UCIParser btime_parser = parser.find_at("btime")) {
                                // go_args.btime = btime_parser.first_word().and_then(str_to_uint);
                                go_args.btime = read_arg(btime_parser);
                        }
                        if (UCIParser binc_parser = parser.find_at("binc")) {
                                // go_args.binc = binc_parser.first_word().and_then(str_to_uint);
                                go_args.binc = read_arg(binc_parser);
                        }
                        if (UCIParser movestogo_parser = parser.find_at("movestogo")) {
                                // go_args.moves_togo = movestogo_parser.first_word().and_then(str_to_uint);
                                go_args.moves_togo = read_arg(movestogo_parser);
                        }
                        if (UCIParser depth_parser = parser.find_at("depth")) {
                                // go_args.depth = depth_parser.first_word().and_then(str_to_uint);
                                go_args.depth = read_arg(depth_parser);
                        }
                        if (UCIParser nodes_parser = parser.find_at("nodes")) {
                                // go_args.nodes = nodes_parser.first_word().and_then(str_to_uint);
                                go_args.nodes = read_arg(nodes_parser);
                        }
                        if (UCIParser mate_parser = parser.find_at("mate")) {
                                // go_args.mate_in = mate_parser.first_word().and_then(str_to_uint);
                                go_args.mate_in = read_arg(mate_parser);
                        }
                        if (UCIParser movetime_parser = parser.find_at("movetime")) {
                                // go_args.move_time = movetime_parser.first_word().and_then(str_to_uint);
                                go_args.move_time = read_arg(movetime_parser);
                        }
                        engine.go(go_args);
                        // start sending information

                } else if (word == "stop") {
                        engine.stop();
                        // the engine sends the info
                } else if (word == "ponderhit") {
                        // after a ponderhit nothing changes, except we leave pondermode
                        engine.ponderhit();
                } else if (word == "quit") {
                        return;
                } else if (word == "ping") {
                        std::cout << "ping\n";
                } else if (word == "d") {
                        std::cout << board2str(engine.get_position()) << "\n"
                                  << "Hash " << std::hex << zobrist_hash(engine.get_position()) << std::dec << std::endl;
                } else if (word == "mkay") {
                        std::cout << "drugs are bad mkay\n";
                } else {
                        std::cerr << "unknown command: \"" << user_input << "\"\n";
                }

                std::cout << std::flush;
        }
}

// todo this should all probably be handled in a UCIHandler class

class UCIInterface {
        ;
};