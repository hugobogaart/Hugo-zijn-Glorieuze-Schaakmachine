#ifndef ENGINE_H
#define ENGINE_H

#include <iostream>

#include "../cli/cli-utils.h"

#include "eval.h"
#include "transtable.h"
#include "zobrist-hash.h"
#include "gen-defs.h"
#include "movegen.h"
#include <cassert>
#include <thread>
#include <memory>

#include <mutex>



struct ThreadArgs {
        bool run;

        // contains the hashes of positions in the line we are looking at
        // not the ones already played on the board
        // used for the repetition rule
        std::vector<uint64_t> hashes_so_far;

        // todo
        std::vector<Position> positions_so_far;
};

const auto empty_thread_id = std::thread::id{};
inline auto is_idle (const std::thread &t) {return t.get_id() == empty_thread_id;}

struct ThreadPool {

        // the engine can start up a search, and than run for 'x' milliseconds
        // after that, a thread stops the engine

        struct TimedEngineStopArgs {
                typedef std::chrono::time_point<std::chrono::steady_clock> time_point_t;

                time_point_t stop_time;
                bool cancel = false;
        };

        std::shared_ptr<TimedEngineStopArgs> to_clock_args = nullptr;

        std::unique_ptr<std::thread[]> worker_threads = nullptr;
        std::unique_ptr<ThreadArgs[]>  worker_args    = nullptr;
        size_t num_threads = 0;

        std::mutex mtx;

        auto empty () const -> bool {return num_threads == 0;}
        auto find_available () const -> std::optional<std::size_t>
        {
                for (size_t i = 0; i < num_threads; i++) {
                        const std::thread &t = worker_threads[i];
                        if (t.get_id() == empty_thread_id) {
                                return i;
                        }
                }
                return std::nullopt;
        }

        auto make_threads (size_t sz)
        {
                if (num_threads == sz)
                        return;

                num_threads = sz;
                worker_threads = std::make_unique<std::thread[]>(sz);
                worker_args    = std::make_unique<ThreadArgs[]>(sz);
        }

        auto workers_active () const -> bool
        {
                for (size_t i = 0; i < num_threads; i++) {
                        if (worker_threads[i].joinable()) {
                                return true;
                        }
                }
                return false;
        }

        ~ThreadPool()
        {
                if (to_clock_args) {
                        to_clock_args->cancel = true;
                        while (to_clock_args.use_count() > 1) {
                                std::this_thread::yield();
                        }
                }
        }
};

// when their engine wants to send info, this is what it is
struct SendInfoArgs {
        // pv
        std::optional<uint64_t> depth   = std::nullopt;
        // no seldepth
        std::optional<uint64_t> time_ms = std::nullopt;
        std::optional<uint64_t> nodes   = std::nullopt;
        std::optional<std::vector<Move>> pv = std::nullopt;
        // no multipv
        struct Score {
                int engine_perspective; // in centi_pawns
                std::optional<int> mate = std::nullopt;
                bool lower_bound;
                bool upper_bound;
        };
        std::optional<Score> score       = std::nullopt;

        std::optional<Move> currmove = std::nullopt;
        std::optional<int> hashfull;
        std::optional<int> nps;
        // no tbhits
        // no cpuloads

        // no other infostrings

        Color active_color;
};

class Engine {
public:

        explicit
        Engine (const Position &pos, void(*send_info_func)(const SendInfoArgs &), void (*send_bestmove_func)(Move mv, Color col), TransTable::MegaByte mb = TransTable::MegaByte(1000))
                : root(pos, zobrist_hash(pos)),
                  tt(mb),
                  current_gen(0),
                  send_info(send_info_func),
                  send_bestmove(send_bestmove_func)
        {
                tt.clear();
        }

        explicit
        Engine (const Position &pos ,TransTable::MegaByte mb = TransTable::MegaByte(1000))
                : root(pos, zobrist_hash(pos)),
                  tt(mb),
                  current_gen(0),
                  send_info(nullptr),
                  send_bestmove(nullptr)
        {
                tt.clear();
        }

        Engine (const Engine &other) = delete;
        Engine operator=(const Engine &other) = delete;

        ~Engine()
        {
                should_send_best_move = false;

                // stop any threads if they are running
                stop();

                // there may be some detached thread

                if (thread_pool.to_clock_args) {
                        thread_pool.to_clock_args->cancel = true;
                        while (thread_pool.to_clock_args.use_count() > 1)
                                std::this_thread::yield();
                }
        }

        struct Options {
                size_t num_threads = 1;
        } opts;

        auto options() -> auto & {return opts;}

        // fills the table until depth (no threads)
        auto fill (int depth) {fill_alpha_beta(depth);}

        // no threads
        auto iterative_deepen(int start_ply, int max_ply) -> void;

        // these functions return the eval/move RIGHT NOW,
        // without regards for what the engine is doing
        auto demand_eval () const -> std::optional<Eval>;
        auto demand_best_move () const ->  std::optional<Move>;

        // these functions allow the engine to finish what they are doing first
        auto request_eval () -> Eval;
        auto request_best_move () -> Move;

        // permille of filled table
        auto filled_permille () const -> int {return 1000 * num_full_nodes / tt.size();}

        struct GoArgs {
                std::optional<MoveList> move_list = std::nullopt;
                bool ponder = false;
                std::optional<size_t> wtime      = std::nullopt;
                std::optional<size_t> btime      = std::nullopt;
                std::optional<size_t> winc       = std::nullopt;
                std::optional<size_t> binc       = std::nullopt;
                std::optional<size_t> moves_togo = std::nullopt;
                std::optional<size_t> depth      = std::nullopt;
                std::optional<size_t> nodes      = std::nullopt;
                std::optional<size_t> mate_in    = std::nullopt;
                std::optional<time_t> move_time  = std::nullopt;
                bool infinite = false;
        };

        // auto position (const Position &pos) -> void;
        auto position (const Position &start_pos, const std::vector<Move> &confirmed_moves) -> void;

        auto go (const GoArgs &go_args) -> void;
        auto stop () -> void;
        auto ponderhit () -> void;
        auto running () const -> bool;


        auto get_hash_size () const -> auto {return tt.size_mb();}
        auto resize_hashtable (TransTable::MegaByte mbs) -> void {tt.resize(mbs);}
        auto get_position () const -> Position {return root.pos;}
private:

        static constexpr int depth_max = std::numeric_limits<int>::max();

        // launches 1 thread that continually runs
        // uses the root restricted moves if the template parameter is set
        template <bool root_restricted = false>
        auto start_iterative_deepen (int max_depth = depth_max) -> void;

        // launches a thread that makes the engine stop after "dur" milliseconds
        auto stop_after (std::chrono::milliseconds dur) -> void;

        // launches a thread that makes the engine stop now
        // the workers can launch this when they are done
        auto launch_worker_killer () -> void;

        // the function that iteratively deepens the search
        // if the template parameter "restrict_root" is set, the root node will use
        // the moves in this->restricted_moves
        template <bool restrict_root = false>
        auto iterative_deepen_thread (int start_depth, int max_depth, const bool &run) -> void;


        // no threads
        // auto fill_recursive(int ply) -> void;
        auto fill_alpha_beta (int depth) -> void;

        // starts 1 thread that continually runs
        // uses the restriction moves from root
        // auto start_iterative_deepen_restricted() -> void;


        // used by threads
        // fills the tt with the alpha beta loop
        // respects the root restriction if applicable
        // like the normal one, but the "run" parameter tells them when to stop
        template <bool restrict_root = false>
        auto fill_alpha_beta_thread (int depth, const bool &run) -> void;
        // auto fill_alpha_beta_restrict_thread (int ply, const bool &run) -> void;


        // auto iterative_deepen_restrict_infinite_thread (int start_ply, const bool &run) -> void;

        // the actual recursive function
        template <Color col>
        auto alpha_beta_col (const PositionHashPair &pos_hash, Eval alpha, Eval beta, int depth_left, const bool &run) -> Eval;

        // this function is like the normal alpha-beta function
        // but the only moves made from the root position are the moves in MoveList this->restricted_moves
        // these are assumed to be valid for the root position in this functions
        template <Color col>
        auto alpha_beta_restricted_root_col (int depth_left, const bool &run) -> Eval;

        // makes an entry in the right bucket in the transposition table
        // chooses which one to overwrite if needed
        auto make_entry (uint64_t hash) -> TransTable::Node *;

        // the central function to obtain a writer to some node in the table
        // 1 -> see if there is already a node with that hash
        // 2 -> try to find and empty node
        // 3 -> replacement
        template <Color col>
        auto get_node_writer(uint64_t hash) -> TransTable::NodeWriter<col>;

        auto active_color() const -> Color {return root.pos.meta.active;}



        // Position root;
        PositionHashPair root;

        // a vector of the hashes of all previously played positions on the board.
        // does NOT include the hash of the root, since that one is handled by the thread vectors
        std::vector<uint64_t> hashes_excluding_root;

        TransTable tt;


        // gen of the current move, every time a move is made, this is incremented
        uint64_t current_gen;
        ThreadPool thread_pool;

        // maybe we have to go search to these moves, by "go <move1> <move2> ..."
        MoveList restricted_moves;
        auto root_restrictions () -> auto & {return restricted_moves;}


        bool pondering = false;

        // each thread gets their own list of moves in which they
        // keep track of the current line
        // std::vector<Move> curr_moves;

        // some information that is being continuously updated and tracked
        // very thread unsafe but whatever

        size_t total_nodes_searched = 0;

        // number of full entries in the tt
        size_t num_full_nodes = 0;

        // timepoint the search started
        std::chrono::time_point<std::chrono::steady_clock> search_start_timepoint;

        // function to send info
        void (* send_info)(const SendInfoArgs &info_args);

        // function to send bestmove
        void (* send_bestmove)(Move mv, Color col);

        bool should_send_best_move = false;
        bool workers_should_kill_themselves = false;

        // for testing functions
        friend auto test_nodegen () -> void;
        friend auto test_threads () -> void;
        friend auto test_resize () -> void;
};

template <Color col>
auto Engine::get_node_writer(uint64_t hash) -> TransTable::NodeWriter<col>
{
        std::optional<TransTable::NodeWriter<col>> opt_writer = tt.find_existing_writer<col>(hash, this->current_gen);
        if (opt_writer)
                return std::move(*opt_writer);

        // the function we use that is going to determine which node is most replaceable
        // returns true <==> n1 is strictly worse than n2
        auto is_worse_than = [&](const TransTable::Node &n1, const TransTable::Node &n2) -> bool {
                if (n1.gen != n2.gen)
                        return n1.gen < n2.gen;

                if (n1.depth_searched != n2.depth_searched)
                        return n1.depth_searched < n2.depth_searched;

                // whatever
                return false;
        };

        return tt.make_replacing_writer<col>(hash, this->current_gen, is_worse_than);
}


template <Color col>
auto Engine::alpha_beta_col (const PositionHashPair &pos_hash, Eval alpha, Eval beta, int depth_left, const bool &run) -> Eval
{
        // todo pass as parameter, for multithreading
        constexpr int thread_id = 0;

        if (!run)
                return 0; // whatever

        // there are 3 ways out of this function
        // 1 -> The position already exists in the transposition table with enough depth
        //      in this case we can skip all the work, it has been done already
        //
        // 2 -> we are a leaf, depth_left == 0
        //      we do a static eval and put that node in the table
        //
        // 3 -> we generate all children, and perform alpha beta reduction
        //      to find the best continuation

        this->total_nodes_searched++;

        constexpr bool is_white = col == Color::white;
        constexpr Eval worst = is_white ? worst_white : worst_black;

        const uint64_t &hash  = pos_hash.hash;

        // the hashes of the positions that have already been made on the board
        const std::vector<uint64_t> &made_hashes = this->hashes_excluding_root;

        // the hashes of the current line
        std::vector<uint64_t> &encountered_hashes = thread_pool.worker_args[thread_id].hashes_so_far;

        // first, we have to make a place in the transposition table
        TransTable::NodeWriter<col> proxy = get_node_writer<col>(hash);

        // we check for repetition
        // there are some different cases
        // -> BOTH moves are still hypothetical
        //      we just return and static_eval a draw (eval 0)
        // -> 0 or 1 of the positions has already been reached on the board
        //      we pretend like that didn't happen, since we need 3 for a draw
        //      concretely, we just ignore it
        // -> this position has already been played twice on the board
        //      any time we reach this position again, it is an instant draw
        //      we write the node as a draw with depth infinity (or indeed the maximum value)
        //
        // so the only time we do something is if the hash has already been seen twice in the made positions
        // or if we encounter it in the hypothetical line twice

        // todo if we have made this position once on the board, and we store the result
        // this is also going to be accidentally used for when is has been made twice
        // solution: something like proxy.mark_repeat() and proxy.is_repeat()?'
        // or at least, when we hit a node we want to not blindly make the move if it is indeed a repetition

        const int already_reached = [&]() -> int {
                int n = 0;
                for (const uint64_t &h : made_hashes) {
                        if (h == hash) {
                                ++n;
                        }
                }
                return n;
        }();

        // already encountered this in the hypothetical line
        const bool encountered_hypo = std::ranges::any_of(encountered_hashes, [&](const uint64_t &x) {return x == hash;});

        // if there is only one encounter we just ignore it
        if (already_reached >= 2) {
                // this is a draw with depth infinity, since this will be the third time
                proxy.write_eval(TransTable::Node::exact, TransTable::Node::max_depth, 0, {});
                proxy.flush();
                return 0;
        }

        // two identical positions in this hypothetical line
        if (encountered_hypo) {
                // draw by repetition
                // if both times this position has been reached are hypothetical, we just flush a static eval
                // however, if one of the positions
                proxy.write_static_eval(0);
                proxy.flush();
                return 0;
        }
        
        // we want to add this position to the hash vector
        // so use a constructor/destructor pair for ease
        struct AddHash{
                AddHash (std::vector<uint64_t> &vec, uint64_t hash)
                        : hashes(vec)
                {
                        hashes.emplace_back(hash);
                }
                ~AddHash ()
                {
                        hashes.pop_back();
                }
                std::vector<uint64_t> &hashes;
        } _(encountered_hashes, hash);

        
        // if we hit a position, either it has already been calculated in sufficient depth,
        // or not. In that case, we'll still believe that top move is worth trying out first,
        // because that is smart with the alpha-beta pruning
        const bool hit = proxy.is_hit();

        // if the position is already sufficiently analyzed, we get an eval.
        // if this eval is exact, we are done
        // if this eval is some limit that is outside the alpha-beta window, we are done as well
        if (hit && proxy.original_depth() >= depth_left) {

                // there is a chance that the move we are about to make blindly
                // is a threefold repetition, making a draw in a winning position
                // this happens when this position stays in the tabld, without being updated
                // we have to check, before returning, if this move will be repetition

                Move mv = proxy.original_move();
                auto next_pos = pos_hash;
                make_move_unsafe<col>(mv, next_pos);
                size_t counter = 0;
                for (const auto &h : made_hashes) {
                        if (h == hash) {
                                ++counter;
                        }
                }
                for (const auto &h : encountered_hashes) {
                        if (h == hash) {
                                ++counter;
                        }
                }

                if (counter < 2) {
                        const typename TransTable::NodeWriter<col>::BoundedEval bounded_eval = proxy.original_eval();

                        if (bounded_eval.ntype == TransTable::Node::NodeType::exact) {
                                proxy.update_gen();
                                proxy.flush();
                                return bounded_eval.eval;
                        } else {
                                if (bounded_eval.ntype == TransTable::Node::NodeType::lowerbound && bounded_eval.eval > beta) {
                                        proxy.update_gen();
                                        proxy.flush();
                                        return bounded_eval.eval;
                                } else if (bounded_eval.ntype == TransTable::Node::NodeType::upperbound && bounded_eval.eval < alpha) {
                                        proxy.update_gen();
                                        proxy.flush();
                                        return bounded_eval.eval;
                                }
                        }
                }
        }

        // we are actually going to write a new node
        if (proxy.was_empty()) {
                num_full_nodes++;
        }

        assert (proxy.node);

        if (depth_left == 0) {

                const Eval eval = static_eval(pos_hash.pos);
                proxy.write_static_eval(eval);
                proxy.flush();

                // best move is not initialised, because if depth-searched is 0, this is not important anyway
                if (tt.find(hash) != proxy.node) {
                        std::cerr << "big error! proxy.node points wrong\n";
                        std::cerr << "found \t" << (void *)tt.find(hash)
                                  << "\npointer\t" << (void *)proxy.node
                                  << "\n first in bucket\t" << (void *)&tt.find_bucket(hash) << std::endl;
                        std::cerr << board2str(pos_hash.pos) << '\n';
                }

                return eval;
        }

        // we are going to generate moves
        // if we hit a node with insufficient depth
        // prev_best_move is likely still the most promising
        // we try this one first because this is advantageous for the pruning

        MoveList move_list;
        //generate_moves2<col>(pos_hash.pos, move_list);
        generate_moves_sorted<col>(pos_hash.pos, move_list);

        // the node holds a valid move if we have a hit, and the eval is not mate and depth > 0
        // swap the best move with the front move, if applicable
        // paranoia: we check if this is indeed a legal move before we do nonsense moves
        if (hit && proxy.original_eval().eval != worst && proxy.original_depth() > 0) {
                const Move prev_best_move = proxy.original_move();

                // swap with front in the list if it indeed exists
                bool valid  = false;
                for (Move &mv : move_list) {
                        if (mv == prev_best_move) {
                                std::swap(mv, move_list[0]);
                                valid = true;
                                break;
                        }
                }

                if (!valid) {
                        // none of the things were apparently this move
                        std::cerr << "invalid move encountered\n";
                        std::cerr << "depth left was " << depth_left;
                        std::cerr << "\nproceeding\n" << std::flush;
                }
        }

        // if the final eval ends up in the window, the eval is exact
        // if the final eval ends up worse than the window, it is only a upper bound(white) / lower bound(black)
        //      because the other color WILL have done a cut-off
        // if the final eval ends up better than the window it is a lower bound(white) / upper bound(black)
        //      because the skipped continuations might have been even better

        // function used to calculate the node type of the eval we eventually calculate
        // in this function
        auto node_type = [alpha, beta] (Eval eval) -> TransTable::Node::NodeType {
                if (eval > beta)
                        return TransTable::Node::NodeType::lowerbound;
                if (eval < alpha)
                        return TransTable::Node::NodeType::upperbound;
                return TransTable::Node::NodeType::exact;
        };

        // we keep track of the best move and (corresponding) eval
        Move best_mv;
        Eval eval = worst;

        auto eval_is_better = [&] (Eval ev) -> bool {
                return is_better_than<col>(ev, eval);
        };

        for (const Move mv : move_list) {
                PositionHashPair poshash_after_move = pos_hash;
                make_move_unsafe<col>(mv, poshash_after_move);
                const Eval sub_eval = alpha_beta_col<!col>(poshash_after_move, alpha, beta, depth_left - 1, run);
                if (eval_is_better(sub_eval)) {
                        eval = sub_eval;
                        best_mv = mv;
                }

                // if the move is "too good", the other player could have already prevented it by force
                // alpha is the minimum score white can force
                // beta is the minimum score (so highest ev) black can force

                // if the move is better than the previous alpha/beta
                // we update alpha/beta to be this new lower limit

                // if all moves are worse than our lower limit, we have seen all next boards
                // these may have been

                if constexpr (is_white) {
                        if (eval > beta) {
                                // we do not care about the other moves
                                // they will be prevented anyway
                                break;
                        }
                        // if not, we keep on searching and improving our window
                        // if we can get a better score than
                        alpha = std::max(alpha, eval);
                } else /* black */ {
                        if (eval < alpha) {
                                break;
                        }
                        beta = std::min(beta, eval);
                }
        }

        // if the eval is mate, we need to increase the ply of the mate
        if (white_is_mated(eval)) {
                ++eval;
        } else if (black_is_mated(eval)) {
                --eval;
        }

        // if there are no moves we are mated
        // or there is stalemate, in which case we have eval 0

        if (move_list.empty()) {
                if (pos_hash.pos.in_check<col>()) {
                        // we are mated
                        // we lose
                        eval = worst;
                } else {
                        // stalemate
                        eval = 0;
                }
        }


        // now we have an eval for this position
        // best mv is not always initialised, but only if we are lost anyway, or depth=0
        // if the movelist is empty, eval will be at the worst

        // we will now write, or abort if (!run)
        // if we abort, we make sure to leave the thing in the state that it was in

        if (!run) {
                proxy.abort();
                return 0; // whatever
        }

        // we write
        proxy.write_eval(node_type(eval), depth_left, eval, best_mv);
        proxy.flush();

        // ancient debug code
        if (tt.find(hash) != proxy.node) {
                std::cerr << "very strange error! to_content points wrong in alpha_beta_col\n";
                std::cerr << "found \t" << (void *)tt.find(hash)
                                  << "\npointer\t" << (void *)proxy.node
                                  << "\nfirst in bucket\t" << (void *)&tt.find_bucket(hash) << std::endl;

                bool bucket_full = tt.find_bucket(hash).find_empty() == nullptr;
                if (bucket_full) {
                        std::cerr << "Bucket is full\nhashes:\n";
                        for (const auto &e : tt.find_bucket(hash).data()) {
                                std::cerr << '\t' << std::hex << e.hash << "\n" << std::dec;
                        }
                } else {
                        std::cerr << "Bucket is empty\n";
                }
                std::cerr << board2str(pos_hash.pos) << '\n';
        }

        return eval;
}

// special case where the moves are already made
template <Color col>
auto Engine::alpha_beta_restricted_root_col (int depth_left, const bool &run) -> Eval
{
        this->total_nodes_searched++;

        constexpr bool is_white = col == Color::white;
        Eval alpha = worst_white;
        Eval beta  = worst_black;
        const MoveList &move_list = this->restricted_moves;
        const uint64_t &hash = this->root.hash;

        auto to_contents = tt.find(hash);

        bool entry_was_in_use = false;

        if (to_contents) {
                if (to_contents->depth_searched >= depth_left) {
                        to_contents->gen = current_gen;
                        return to_contents->eval;
                }
                entry_was_in_use = to_contents->in_use;

        } else {
                to_contents = make_entry(hash);
        }

        bool entry_was_empty = false;
        if (to_contents->hash == 0) {
                // this was apparently an empty node
                entry_was_empty = true;
                num_full_nodes++;
        }

        to_contents->hash = hash;
        to_contents->in_use = true;

        Eval worst = white_black<col>(worst_white, worst_black);
        Eval eval = worst;
        Move best_move = move_list[0];  // assume there is something there

        auto is_better = [&] (Eval ev) -> bool {
                return white_black<col>(ev > eval, ev < eval);
        };

        // standard alpha-beta loop
        for (const Move mv : this->restricted_moves) {
                if (!run)
                        return 0;

                PositionHashPair poshash_after_move = this->root;
                make_move_unsafe<col>(mv, poshash_after_move);
                const Eval sub_eval = alpha_beta_col<!col>(poshash_after_move, alpha, beta, depth_left - 1, run);
                if (is_better(sub_eval)) {
                        eval = sub_eval;
                        best_move = mv;
                }

                if constexpr (is_white) {
                        if (eval > beta) {
                                break;
                        }
                        alpha = std::max(alpha, eval);
                } else /* black */ {
                        if (eval < alpha) {
                                break;
                        }
                        beta = std::min(beta, eval);
                }
        }

        if (entry_was_in_use == false)
                to_contents->in_use = false;

        if (!run) {
                if (entry_was_empty)
                        to_contents->hash = 0;
                return 0;
        }

        to_contents->hash = hash;
        to_contents->gen = current_gen;
        to_contents->eval = eval;
        to_contents->best_move = best_move;
        to_contents->depth_searched = depth_left;

        return eval;
}

template <bool restrict_root>
auto Engine::fill_alpha_beta_thread (int depth, const bool &run) -> void
{
        bool white_start = root.pos.meta.active == Color::white;
        if constexpr (restrict_root) {
                if (white_start) {
                        (void)alpha_beta_restricted_root_col<Color::white>(depth, run);
                } else {
                        (void)alpha_beta_restricted_root_col<Color::black>(depth, run);
                }
        } else /* normal alpha-beta start, root unrestricted */ {
                if (white_start) {
                        (void)alpha_beta_col<Color::white>(root, worst_white, worst_black, depth, run);
                } else {
                        (void)alpha_beta_col<Color::black>(root, worst_white, worst_black, depth, run);
                }
        }
}

template <bool restrict_root>
auto Engine::iterative_deepen_thread(int start_depth, int max_depth, const bool &run) -> void
{
        while (run && start_depth <= max_depth) {
                ++current_gen;
                fill_alpha_beta_thread<restrict_root>(start_depth++, run);
                if (!run || send_info == nullptr)
                        continue;

                // send info
                SendInfoArgs args;
                // todo pv

                const auto to_root = tt.find(root.hash);
                if (to_root == nullptr)
                        continue;


                // time
                std::chrono::duration dur = std::chrono::steady_clock::now() - search_start_timepoint;
                args.time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();

                Color active = active_color();
                bool is_white = active == Color::white;
                args.active_color = active;
                args.hashfull = filled_permille();
                args.depth = start_depth - 1;

                SendInfoArgs::Score score;

                score.engine_perspective = is_white ? to_root->eval : -to_root->eval;
                score.lower_bound = to_root->node_type == TransTable::Node::lowerbound;
                score.upper_bound = to_root->node_type == TransTable::Node::upperbound;
                if (is_mate(to_root->eval)) {
                        int num_plies;
                        if (white_is_mated(to_root->eval))
                                num_plies = to_root->eval - worst_white;
                        else
                                num_plies = worst_black - to_root->eval;

                        bool opponent_is_mated = is_white == black_is_mated(to_root->eval);

                        // the amount of moves to the mate, not plies
                        // int num_moves = opponent_is_mated ? (1 + num_plies) / 2 : num_plies / 2;
                        int num_moves = (1 + num_plies) / 2;

                        score.mate = opponent_is_mated ? num_moves : -num_moves;
                }

                // args.pv =
                /*
                args.score = SendInfoArgs::Score{};
                args.score->engine_perspective = active == Color::white ? request_eval() : -request_eval();
                args.score->mate = std::nullopt;        // yet
                args.score->lower_bound = false;
                args.score->upper_bound = false;
                */
                args.score = score;

                send_info(args);


        }
}

template <bool restrict_root>
auto Engine::start_iterative_deepen(int max_depth) -> void
{
        constexpr int start_depth = 0;

        if (thread_pool.workers_active())
                return; // already doing something?

        thread_pool.make_threads(1);

        // threads can't immediately bind to a member function
        auto worker = [=] {
                bool &thread_run = this->thread_pool.worker_args[0].run;
                thread_run = true;
                this->iterative_deepen_thread<restrict_root>(start_depth, max_depth, thread_run);

                // if there is a maximum depth, and stop() is not called
                // the threads are going to "float" around so to speak
                // the threads can of course not stop themselves
                // this function kills the worker(s), but can be called by the workers
                if (this->workers_should_kill_themselves) {
                        this->workers_should_kill_themselves = false;
                        launch_worker_killer();
                }
        };

        // there is no other thread that is going to stop these threads when they have
        // reached the desired depth
        if (max_depth != Engine::depth_max)
                this->workers_should_kill_themselves = true;

        thread_pool.worker_threads[0] = std::thread(worker);
}

#endif //ENGINE_H
