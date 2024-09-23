//
// Created by Hugo Bogaart on 22/07/2024.
//

#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#include <memory>
#include <vector>
#include <algorithm>

template <typename T>
struct MemoryPool {
        constexpr explicit
        MemoryPool(size_t size)
                : space (size),
                  freespace(size)
        {
                for (size_t i = 0; i < freespace.size(); i++) {
                        freespace[i] = false;
                }
        }

        std::vector<T>    space;
        std::vector<bool> freespace;
        size_t cached_index = 0;
};

// must meet requirements of Allocator https://en.cppreference.com/w/cpp/named_req/Allocator
// is a stateful allocator
template <typename T>
struct PoolAllocator {
        using value_type = T;
        using pointer_type = T*;

        constexpr explicit
        PoolAllocator (MemoryPool<T> &pl) : pool(pl)
        { }

        constexpr
        PoolAllocator (const PoolAllocator &other) : pool(other.pool)
        { }

        ~PoolAllocator() = default;

        constexpr
        auto allocate (size_t n) -> pointer_type
        {
                std::optional<size_t> block = find_block(n);
                if (!block)
                        throw std::bad_alloc();
                allocate_block_unsafe(*block, n);
                return &pool.space[*block];
        }

        constexpr
        auto deallocate (pointer_type p, size_t n) -> void
        {
                deallocate_block_unsafe(p - pool.space.data(), n);
        }


private:

        constexpr
        auto find_block (size_t n) -> std::optional<size_t>
        {

                size_t start_block_index;
                size_t in_a_row = 0;

                for (size_t i = pool.cached_index; i < pool.freespace.size(); i++) {
                        if (pool.freespace[i]) {
                                in_a_row = 0;
                                continue;
                        }
                        // free element
                        if (in_a_row == 0)
                                start_block_index = i;
                        in_a_row++;
                        if (in_a_row == n) {
                                pool.cached_index = i;
                                return start_block_index;
                        }
                }

                // maybe we just start over
                if (pool.cached_index != 0) {
                        pool.cached_index = 0;
                        return find_block(n);
                }
                return std::nullopt;
        }

        constexpr
        auto allocate_block_unsafe (size_t index, size_t n)
        {
                for (size_t i = index; i < index + n; i++) {
                        pool.freespace[i] = true;
                }
        }

        constexpr
        auto deallocate_block_unsafe (size_t index, size_t n)
        {
                for (size_t i = index; i < index + n; i++) {
                        pool.freespace[i] = false;
                }
        }

        MemoryPool<T> &pool;
};


#endif //ALLOCATORS_H
