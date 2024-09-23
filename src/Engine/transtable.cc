//
// Created by Hugo Bogaart on 27/07/2024.
//

#include "transtable.h"
#include "position.h"
#include <cstdint>
#include <algorithm>


TransTable::TransTable(TransTable&& other) noexcept
        : num_buckets(std::exchange(other.num_buckets, 0)),
          table(std::move(other.table))
{ }

TransTable &TransTable::operator=(TransTable&& other) noexcept
{
        num_buckets = std::exchange(other.num_buckets, 0);
        table = std::move(other.table);
        return *this;
}

auto TransTable::resize (MegaByte mbs) -> void
{
        // naive way, just make new table and copy everything in there
        TransTable new_tt(mbs);

        assert (new_tt.calculate_num_full() == 0);

        for (const Bucket &buck : *this) {
                for (const Node &cnt : buck.data()) {
                        if (cnt.hash == 0)
                                continue;
                        // copy the content into the new tt
                        auto &new_buck = new_tt.find_bucket(cnt.hash);
                        *new_buck.find_empty() = cnt;
                }
        }

        assert (new_tt.calculate_num_full() == calculate_num_full());
        *this = std::move(new_tt);
}
