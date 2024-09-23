#include "engine.h"
#include "transtable.h"
#include "movegen.h"


auto Engine::fill_alpha_beta (int depth) -> void
{
        bool run = true; // for the threads (we don't use that in this function)
        if (root.pos.meta.active == Color::white) {
                (void)alpha_beta_col<Color::white>(root, worst_white, worst_black, depth, run);
        } else {
                (void)alpha_beta_col<Color::black>(root, worst_white, worst_black, depth, run);
        }
}

auto Engine::demand_eval () const -> std::optional<Eval>
{
        auto p = tt.find(root.hash);
        if (p == nullptr)
                return std::nullopt;
        return p->eval;
}

auto Engine::demand_best_move () const -> std::optional<Move>
{
        auto p = tt.find(root.hash);
        if (p == nullptr)
                return std::nullopt;
        return p->best_move;
}

auto Engine::request_eval () -> Eval
{
        if (auto maybe_eval = demand_eval())
                return *maybe_eval;

        // for some weird reason there is no root node in the transposition table
        std::cerr << "could not find a root position in the transposition table for some reason\n";
        fill(1);
        if (auto maybe_eval = demand_eval())
                return *maybe_eval;

        // or not
        // this should absolutely never happen
        throw std::runtime_error("no move found");
}

auto Engine::request_best_move () -> Move
{
        if (auto maybe_best_move = demand_best_move())
                return *maybe_best_move;

        // for some weird reason there is no root node in the transposition table
        std::cerr << "could not find a root position in the transposition table for some reason\n";
        fill(1);
        if (auto maybe_best_move = demand_best_move())
                return *maybe_best_move;


        // or not
        // this should absolutely never happen
        throw std::runtime_error("no move found");
}


auto Engine::make_entry (uint64_t hash) -> TransTable::Node *
{
        // first we try to get a space that has not been used yet
        TransTable::Bucket &buck = tt.find_bucket(hash);
        TransTable::Node *to_content = buck.find_empty();

        if (to_content) {
                // apparently there is an empty space for us
                return to_content;
        }

        // we have to throw out one of the old ones
        // we just throw away the oldest
        // in case of doubt, the one with the least depth
        // still in doubt, ... whatever

        const auto begin = buck.data().begin();
        const auto end = buck.data().end();

        auto is_worse = [&] (decltype(to_content) p) -> bool {
                if (p->gen != to_content->gen)
                        return p->gen < to_content->gen;

                if (p->depth_searched != to_content->depth_searched)
                        return p->depth_searched < to_content->depth_searched;

                // whatever
                return false;
        };

        to_content = begin;
        // to_content may NEVER point to an entry that is currently in use
        // unless the hash is the same, but in that case this function should never be called anyway
        while (to_content->in_use && to_content < end)
                to_content++;

        for (auto cand = to_content + 1; cand < end; cand++) {
                if (cand->in_use)
                        continue;

                if (is_worse(cand)) {
                        to_content = cand;
                }
        }

        if (to_content->in_use)
                std::cerr << "forced to replace a in-use entry\n";

        return to_content;
}

auto Engine::stop () -> void
{
        // we only let one thread handle the threads at a time
        std::lock_guard _(thread_pool.mtx);

        // because we already did that
        this->workers_should_kill_themselves = false;

        // tell the threads to stop working
        for (int i = 0; i < thread_pool.num_threads; i++) {
                thread_pool.worker_args[i].run = false;
        }

        // and join them in
        for (int i = 0; i < thread_pool.num_threads; i++) {
                if (thread_pool.worker_threads[i].joinable()) {
                        thread_pool.worker_threads[i].join();
                }
        }

        if (this->send_bestmove != nullptr && this->should_send_best_move){
                this->should_send_best_move = false;
                send_bestmove(request_best_move(), active_color());
        }
}

auto Engine::iterative_deepen(int start_ply, int max_ply) -> void
{
        while (start_ply <= max_ply) {
                ++current_gen;
                fill_alpha_beta(start_ply++);
        }
}

/*
auto Engine::position (const Position &pos) -> void
{
        this->root.pos = pos;
        this->root.hash = zobrist_hash(pos);
}
*/

auto Engine::position (const Position &start_pos, const std::vector<Move> &confirmed_moves) -> void
{
        hashes_excluding_root.clear();
        PositionHashPair pos_hash(start_pos, zobrist_hash(start_pos));
        for (Move mv : confirmed_moves) {
                hashes_excluding_root.emplace_back(pos_hash.hash);
                if (pos_hash.pos.meta.active == Color::white) {
                        make_move_unsafe<Color::white>(mv, pos_hash);
                } else {
                        make_move_unsafe<Color::black>(mv, pos_hash);
                }
        }
        this->root = pos_hash;
}

auto Engine::go (const GoArgs &args) -> void
{
        // if we are already doing something we just ignore this call
        if (running())
                return;

        // todo

        // add restriction moves if applicable
        this->restricted_moves.clear();
        if (args.move_list) {
                // we verify the moves
                MoveList allowed_moves;
                if (active_color() == Color::white)
                        generate_moves<Color::white>(root.pos, allowed_moves);
                else
                        generate_moves<Color::black>(root.pos, allowed_moves);

                // all valid moves are in the allowed moves list
                auto is_valid = [&] (Move mv) -> bool {
                        return std::ranges::any_of(allowed_moves, [&](Move allowed) {
                                return allowed == mv;
                        });
                };

                for (Move candidate : *args.move_list) {
                        if (is_valid(candidate)) {
                                restricted_moves.emplace_back(candidate);
                        }
                }
        }

        // if we are pondering, the position is "fake" in the sense that
        // it only becomes real if the opponent plays the "pomdering" move that was given in position
        // so we are thinking during the time of the opponent now
        // pondering only effects time management

        /*
         this->pondering = args.ponder;
        */

        // calculation time
        std::optional<std::chrono::milliseconds> calculation_time;

        if (args.move_time) {
                // the time is given in milliseconds
                calculation_time = std::chrono::milliseconds(*args.move_time);
        } else if (args.infinite) {
                calculation_time = std::nullopt;
        } else {
                // calculate time

                calculation_time = std::nullopt;
        }

        // set hashes
        for (size_t t = 0; t < thread_pool.num_threads; ++t) {
                ThreadArgs &targs = thread_pool.worker_args[t];
                // targs.hashes_so_far = hashes_excluding_root;
                targs.hashes_so_far.clear();
                targs.positions_so_far.clear();
        }


        if (calculation_time) {
                stop_after(*calculation_time);
        }


        int max_depth = args.depth.value_or(Engine::depth_max);

        // if args.infinity is set, or args.move_time is not given
        // we calculate indefinately

        search_start_timepoint = std::chrono::steady_clock::now();
        this->should_send_best_move = true;

        // depending on if we have restricted moves
        if (this->restricted_moves.empty()) {
                start_iterative_deepen<false>(max_depth);
        } else {
                start_iterative_deepen<true>(max_depth);
        }
}

auto Engine::stop_after (std::chrono::milliseconds dur) -> void
{
        auto time_stop_worker = [this]() -> void {
                const std::shared_ptr to_args = this->thread_pool.to_clock_args;
                while (true) {
                        if (to_args->cancel)
                                return;

                        const auto now = std::chrono::steady_clock::now();

                        if (now >= to_args->stop_time) {
                                // we stop the engine
                                this->stop();
                                return;
                        }
                        std::this_thread::sleep_for(std::chrono::microseconds(500));
                }
        };

        // there are 2 cases, either there is already a timer thread active, or not
        // if not, we make a new struct, otherwise we update the other one

        if (this->thread_pool.to_clock_args) {
                // already exists
                this->thread_pool.to_clock_args->cancel = true;
                // now we wait for the thread to exit
                while (this->thread_pool.to_clock_args.use_count() > 1)
                        std::this_thread::yield();

        } else {
                // make new one
                this->thread_pool.to_clock_args = std::make_shared<ThreadPool::TimedEngineStopArgs>();
        }

        this->thread_pool.to_clock_args->cancel = false;
        this->thread_pool.to_clock_args->stop_time = std::chrono::steady_clock::now() + dur;

        std::thread(time_stop_worker).detach();
}

auto Engine::launch_worker_killer () -> void
{
        auto worker_killer = [this]() -> void {
                // we copy these args so that the shared_ptr shows a different user
                const std::shared_ptr to_args = this->thread_pool.to_clock_args;
                this->stop();
        };

        // if there is a timer thread active, we cancel that

        if (this->thread_pool.to_clock_args) {
                // already exists
                this->thread_pool.to_clock_args->cancel = true;
                // now we wait for the thread to exit
                while (this->thread_pool.to_clock_args.use_count() > 1)
                        std::this_thread::yield();

        } else {
                // make new one
                this->thread_pool.to_clock_args = std::make_shared<ThreadPool::TimedEngineStopArgs>();
        }

        this->thread_pool.to_clock_args->cancel = false;
        std::thread(worker_killer).detach();
}


auto Engine::running() const -> bool
{
        const std::thread *begin = this->thread_pool.worker_threads.get();
        const std::thread *end = begin + this->thread_pool.num_threads;
        return std::any_of(begin, end, [](const std::thread &t) -> bool {
                return !is_idle(t);
        });
}


auto Engine::ponderhit () -> void
{
        pondering = false;
}