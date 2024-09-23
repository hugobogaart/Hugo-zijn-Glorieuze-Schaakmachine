/*
 * this file is mainly to define the lookup tables
 */

#include "position.h"

struct LookuptableFuncs {
        static consteval auto make_horse_jumps(OneSquare x) -> Field
        {
                Field ret = 0;
                ret |= x << 15 & msk::RIGHT; // nnw
                ret |= x << 17 & msk::LEFT; // nne
                ret |= x << 10 & msk::LEFT2; // nee
                ret |= x << 6 & msk::RIGHT2; // nww
                ret |= x >> 15 & msk::LEFT; // sse
                ret |= x >> 17 & msk::RIGHT; // ssw
                ret |= x >> 10 & msk::RIGHT2; // sww
                ret |= x >> 6 & msk::LEFT2; // see
                return ret;
        }

        static consteval auto make_horse_jump_lookup_table() -> std::array<Field, 64>
        {
                std::array<Field, 64> arr{};
                for (OneSquare s : all_squares)
                        arr[square_to_shift(s)] = make_horse_jumps(s);
                return arr;
        }

        static consteval auto make_king_area(OneSquare sq) -> Field
        {
                Field area = sq;

                area |= shifted<north>(area);
                area |= shifted<south>(area);

                //                      0 1 0
                // area looks like      0 1 0   (unless edge)
                //                      0 1 0


                area |= shifted<east>(area);
                area |= shifted<west>(area);
                return area;
        }

        static consteval auto make_king_area_lookup_table() -> std::array<Field, 64>
        {
                std::array<Field, 64> arr{};
                for (OneSquare s : all_squares)
                        arr[square_to_shift(s)] = make_king_area(s);
                return arr;
        }
};


constexpr std::array<Field, 64> horse_jumps_table = LookuptableFuncs::make_horse_jump_lookup_table();
constexpr std::array<Field, 64> king_area_lookup_table = LookuptableFuncs::make_king_area_lookup_table();
