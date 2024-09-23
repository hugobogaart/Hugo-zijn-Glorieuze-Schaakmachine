//
// Created by Hugo Bogaart on 27/07/2024.
//

#ifndef TRANSTABLE_H
#define TRANSTABLE_H

#include <cstdint>
#include "position.h"
#include "memory"
#include <array>
#include "eval.h"

// for cerr
#include <iostream>

constexpr size_t bytes_in_mb = 1ull << 20;


// hashtable with chess boards as keys via the zobrist hash function
class TransTable {
        // todo alignment? std::aligned_alloc or something
        static constexpr size_t elems_in_bucket = 4;
public:

        struct Node {

                enum NodeType {
                        exact,          // all evals were within [alpha, beta], so the score is exact
                        lowerbound,     // score is an upper bound
                        upperbound      // score is lower bound
                };

                // a hash of 0 marks empty content
                uint64_t hash;          // full hash code
                Eval eval;
                Move best_move;  // best move the last evaluation
                uint8_t depth_searched : 6;        // depth 0 is static eval
                NodeType node_type : 2; // bound type

                // implementation details
                uint8_t gen : 7;    // "generation" this node was added in

                // access flag
                // if this bit is set, some thread currently has this Node open in the callstack
                // this means the data of the node is inconsistent and going to be overriden
                bool in_use : 1 = false;

                static constexpr int max_depth = (1 << 6) - 1; // because 6 bits

                Node () : hash(0) { }

                auto is_exact () const -> bool {return node_type == exact;}
                auto get_depth_searched () const -> uint8_t {return depth_searched;}
        };

        // struct to access a node
        // it keeps internal state and writes everything on destruction
        template <Color col>
        struct NodeWriter {
        private:
                // only the transtable can make these
                // we need search depth for the hit type
                explicit NodeWriter (Node *to_node, uint64_t hash, int gen)
                        : original(*to_node), buffer(*to_node), node(to_node), write_on_exit(false)
                {
                        // we have to specify the type of hit we have in the table
                        if (node->hash == 0) {
                                hit_type = empty;
                        } else if (original.hash == hash) {
                                hit_type = hit;
                        } else {
                                hit_type = replace;
                        }

                        // "claim" the node if it is empty
                        // this prevents some other function from taking this one as well
                        // functions deeper in the callstack with the same hash will see this node
                        // but they will always have a lower depth to go, so we can replace them without losing information
                        node->hash = hash;
                        node->in_use = true;

                        buffer.gen = gen;
                        // buffer.in_use = original.in_use; already done
                        buffer.hash = hash;
                }

                static constexpr Eval worst = white_black<col>(worst_white, worst_black);
                static constexpr bool is_white = col == Color::white;


                enum HitType {
                        hit,        // the node has the same hash
                        replace,    // we replaced an old node
                        empty       // previous node was empty
                } hit_type;

                friend class TransTable;
        public:

                // introduce move semantics to make sure
                // only one writer can access the node
                NodeWriter (const NodeWriter &other) = delete;
                NodeWriter &operator=(const NodeWriter &other) = delete;

                // we transfer the "ownership" of the Writer
                // so that when the other is destroyed, it does not do anything with the node
                NodeWriter (NodeWriter &&other) noexcept
                        : original(other.original), buffer(other.buffer), hit_type(other.hit_type),
                          node(std::exchange(other.node, nullptr)), write_on_exit(other.write_on_exit)
                { }

                NodeWriter &operator=(NodeWriter &&other) noexcept
                {
                        original = other.original;
                        buffer = other.buffer;
                        hit_type = other.hit_type;
                        write_on_exit = other.write_on_exit;
                        node = std::exchange(other.node, nullptr);
                        return *this;
                }

                // returns true if the node previously held this position as well
                // and also holds valid results, so no reserved node that still has to be filled in
                // with all the data except hash
                auto is_hit () const -> bool {return hit_type == hit && !original.in_use;}

                auto was_empty () const -> bool {return hit_type == empty;}
                auto get_hit_type () const -> HitType {return hit_type;}

                struct BoundedEval {
                        Node::NodeType ntype;
                        Eval eval;
                };

                // only to be used when it makes sense
                // js valid if is_hit() has been called and true
                auto original_eval () const -> BoundedEval
                {
                        assert (hit_type == hit && !original.in_use);
                        BoundedEval eval;
                        eval.eval = original.eval;
                        eval.ntype = original.node_type;
                        return eval;
                }

                // only to be used when it makes sense
                auto original_move () const -> Move
                {
                        assert (is_hit());
                        assert (original.eval != worst);
                        assert (original.depth_searched > 0);
                        return original.best_move;
                }

                auto original_depth () const -> int
                {
                        assert (hit_type == hit && !original.in_use);
                        return original.depth_searched;
                }

                /*
                // if the eval was calculated deep enough we return it
                [[deprecated("Use original_eval")]]
                auto maybe_eval () const -> std::optional<BoundedEval>
                {
                        if (hit_type == hit && !original.in_use)
                                return BoundedEval{original.node_type, original.eval};
                        return std::nullopt;
                }

                // returns the best move if it means anything
                [[deprecated("Use original_move")]]
                auto maybe_best_move () const -> std::optional<Move>
                {
                        if (is_hit() && !original.in_use && original.eval != worst && original.depth_searched > 0)
                                return original.best_move;
                        return std::nullopt;
                }
                */

                auto flush ()
                {
                        *node = buffer;
                        write_on_exit = false;
                        original = buffer;
                }

                ~NodeWriter()
                {
                        // maybe we do not own the node
                        if (node == nullptr)
                                return;

                        *node = write_on_exit ? buffer : original;
                        if (write_on_exit) {
                                std::cerr << "flush was not called on the NodeWriter\n";
                        }
                }

                // aborts the write, makes sure we don't write nonsense data
                auto abort () {write_on_exit = false;}

                auto write_static_eval (Eval eval)
                {
                        buffer.eval = eval;
                        buffer.node_type = Node::NodeType::exact;
                        buffer.depth_searched = 0;

                        // buffer.mv does not have to be initialised
                        write_on_exit = true;
                }

                // used when we had a hit
                auto update_gen () -> void
                {
                        // we already updated the gen in the constructor
                        // the rest is identical to the original
                        assert (hit_type == hit);
                        write_on_exit = true;
                }

                auto write_eval (Node::NodeType ntype, int depth, Eval eval, Move best_mv)
                {
                        buffer.node_type = ntype;
                        buffer.depth_searched = depth;
                        buffer.eval = eval;
                        buffer.best_move = best_mv;
                        write_on_exit = true;
                }

                Node original;
                // internal Node that we work in
                Node buffer;
                Node *node;

                // whether we discard the changes on write
                // if discard == true we restore the original
                bool write_on_exit;
        };

        struct /* alignas (64) */  Bucket {

                auto data () -> auto & {return entries;}
                auto data () const -> const auto & {return entries;}


                // returns a pointer to the element with the same hash, or nullptr
                // no optional references are allowed. Too bad!
                auto contains (uint64_t hash) -> Node *
                {
                        Node *it = std::ranges::find_if(entries, [&](const Node &cnt) {return cnt.hash == hash;});
                        return it == entries.end() ? nullptr : it;
                }


                auto contains (uint64_t hash) const -> const Node *
                {
                        const Node *it = std::ranges::find_if(entries, [&](const Node &cnt) {return cnt.hash == hash;});
                        return it == entries.end() ? nullptr : it;
                }

                // returns pointer to first empty Node or nullptr
                auto find_empty () -> Node *
                {
                        return contains(0ull);
                }

                // with 4 Contents in the bucket the bucket is exactly 64 bytes
                std::array<Node, elems_in_bucket> entries;
        };

        // helper class for hashtable sizing
        struct MegaByte {
                size_t num_mbs;
                explicit MegaByte (size_t num) : num_mbs(num) { }

                // to number of buckets
                // 1MB = 1000000 byte
                // or    2^20 for some reason
                explicit operator size_t () const {return bytes_in_mb * num_mbs / sizeof (Bucket);}
        };


        explicit TransTable(size_t num_elems)
                : num_buckets(num_elems / elems_in_bucket),
                  table(std::make_unique<Bucket[]>(num_elems / elems_in_bucket))
        {
                clear();
        }

        explicit TransTable(MegaByte mbs)
                : num_buckets(static_cast<size_t>(mbs)),
                  table(std::make_unique<Bucket[]>(num_buckets))
        {
                clear();
        }

        TransTable (const TransTable &other) = delete;
        TransTable &operator= (const TransTable &other) = delete;
        TransTable (TransTable &&other) noexcept;
        TransTable &operator= (TransTable &&other) noexcept;

        auto size () const -> size_t {return num_buckets * elems_in_bucket;}

        auto size_mb () const -> MegaByte
        {
                return MegaByte(num_buckets * sizeof (Bucket) / bytes_in_mb);
        }

        auto find_bucket (uint64_t hash) -> Bucket & {return table[hash % num_buckets];}

        auto find_bucket (uint64_t hash) const -> const Bucket & {return table[hash % num_buckets];}

        auto find (uint64_t hash) -> Node *
        {
                // too bad optional references are not allowed
                Bucket &buck = find_bucket(hash);
                return buck.contains(hash);
        }

        auto find (uint64_t hash) const -> const Node *
        {
                const Bucket &buck = find_bucket(hash);
                return buck.contains(hash);
        }

        // tries to return a writer to an existing node
        template <Color col>
        auto find_existing_writer (uint64_t hash, int gen) -> std::optional<NodeWriter<col>>
        {
                auto existing = find(hash);
                if (existing == nullptr)
                        return std::nullopt;
                return NodeWriter<col>(existing, hash, gen);
        }

        // tries to make a writer in an empty spot
        // forces a replacement if necessary and returns a writer
        // does NOT try to find an existing hash
        template<Color col, class ComparisonFunction>
        auto make_replacing_writer (uint64_t hash, int gen, ComparisonFunction &&is_worse) -> NodeWriter<col>
        {
                // f(n1, n2) is true <==> n1 is strictly worse than n2
                Bucket &buck = find_bucket(hash);
                Node *to_node = buck.find_empty();
                if (to_node)
                        return NodeWriter<col>(to_node, hash, gen);

                to_node = buck.data().begin();

                // if they are in use we skip them
                // in principle we would not if the hash is the same, but that is not gonna happen anyway
                while (to_node->in_use && to_node < buck.data().end())
                        to_node++;

                for (auto cand = to_node + 1; cand < buck.data().end(); cand++) {
                        if (cand->in_use)
                                continue;

                        if (is_worse(*cand, *to_node)) {
                                to_node = cand;
                        }
                }

                if (to_node->in_use)
                        std::cerr << "forced to replace a in-use entry\n";

                return NodeWriter<col>(to_node, hash, gen);
        }

        auto clear () -> void
        {
                for (Bucket &buck : *this) {
                        for (Node &cnt : buck.data()) {
                                cnt.hash = 0;
                        }
                }
        }
        
        auto calculate_num_empty () const -> size_t
        {
                size_t num = 0;
                for (const Bucket &buck : *this) {
                        for (const Node &cnt : buck.data()) {
                                if (cnt.hash == 0) {
                                        num++;
                                }
                        }
                }
                return num;
        }

        auto calculate_num_full () const -> size_t {return size() - calculate_num_empty();}

        auto resize (MegaByte mbs) -> void;

        Bucket *begin() {return table.get();}
        const Bucket *begin() const {return table.get();}
        Bucket *end() {return begin() + num_buckets;}
        const Bucket *end() const {return begin() + num_buckets;}

// private:

        size_t num_buckets;
        std::unique_ptr<Bucket[]> table;
};

// number of elements in a tt of a gigabyte
constexpr size_t gigabyte_tt = (1ull << 30) / sizeof (TransTable::Node);



#endif //TRANSTABLE_H
