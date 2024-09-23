#ifndef BOT_GEN_DEFS_H
#define BOT_GEN_DEFS_H

#include <chrono>
#include <bit>
#include <cstdint>

#ifdef __BMI2__
#include <x86intrin.h>
#include <bmi2intrin.h>
#endif

// return number of bits turned to 1
// http://en.wikipedia.org/wiki/Hamming_weight

// returns number of 0-bits from the least significant position

#if defined(__GNUC__) || defined(__clang__)
constexpr
auto bit_count(uint64_t x) -> int
{
        return __builtin_popcountll(x);
        // or std::bitcount(x)
}

constexpr
auto trailing_0_count(uint64_t not_zero) -> int
{
        return __builtin_ctzll(not_zero);
}

#elif defined(_MSC_VER) && defined(_WIN64)
#include <intrin.h>
constexpr
auto bit_count(uint64_t x) -> int
{
        return __popcnt64(x);
}


constexpr
auto trailing_0_count(uint64_t x) -> int
{
        return std::countr_zero<uint64_t>(x);
}
#else

constexpr
auto bit_count(uint64_t x) -> int
{
        x -= (x >> 1) & 0x5555555555555555;
        x  = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
        x  = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0f;
        return (x * 0x0101010101010101) >> 56;
}

constexpr
auto trailing_0_count(uint64_t x) -> int
{
        return std::countr_zero<uint64_t>(x);
}

#endif



// say is a bitmask of squares a piece can move to, for instance all bishop moves.
// some of these squares can be blocked, and depending on these squares, the piece is restricted in movement
// there are a bunch of configurations of how there can be obstacles on these bishop squares
// if we want to know exactly to which squares a piece can go to, a lookup table is useful
// (obstacles configuration) -> movement
// so given a bitmask of squares, and a bunch of obstacles we want to map the obstacles to
// the least significant bits, to use as an index
//
// this is what the PEXT instruction does, but it is only supported on x86

// |a|b|c|d|e|f|g|h|            input 1
// |1|0|1|0|1|1|0|1|            input 2
//      ->
// |0|0|0|a|c|e|f|h|            output

// in our case, input 2 (the mapping bits) is exactly the in_the_way

// this code emulates the PEXT instruction


// code from Matthew Fioravante https://github.com/fmatthew5876/stdcxx-bitops/blob/master/include/bitops.hh
inline
auto pext (const uint64_t &input, const uint64_t &bit_idc) -> uint64_t {
#ifdef __BMI2__ // bmi2 instruction set for pext and pdep
        return _pext_u64(input, bit_idc);
#else

        uint64_t res = 0;
        uint64_t mask = bit_idc;
        uint64_t bb = 1;
        do {
                uint64_t lsb = mask & -mask;
                mask &= ~lsb;
                bool isset = input & lsb;
                res |= isset ? bb : 0;
                bb += bb;
        } while (mask);
        return res;
#endif

}


// the opposite of PEXT is PDEP
// PDEP deposits bits with 0's inbetween

inline
auto pdep (const uint64_t &input, const uint64_t &bit_idc) -> uint64_t
{
#ifdef __BMI2__
        return _pdep_u64(input, bit_idc);
#else
        uint64_t result = 0;
        for (int src_pos = 0, mask_pos = 0; mask_pos != 64; ++mask_pos) {
                if (bit_idc >> mask_pos & 1ull) {
                        result |= (input >> src_pos++ & 1ull) << mask_pos;
                }
        }
        return result;
#endif
}

// used in some functions to switch between
// explicit calculation and using a lookup table
enum struct CalculationType {
        calculation, lookup_table
};

consteval auto is_calculation (CalculationType ct) -> bool {return ct == CalculationType::calculation;}
consteval auto is_lookup (CalculationType ct) -> bool {return ct == CalculationType::lookup_table;}


enum class Color : bool {
        black, white
};

constexpr Color operator!(Color color)
{
        return color == Color::white ? Color::black : Color::white;
}

// chooses variable depending on color of the template parameter
template <Color col, typename T>
constexpr T white_black(const T &white_arg, const T &black_arg)
{
        return col == Color::white ? white_arg : black_arg;
}

// timer class
// times the constructor to destructor, and puts the timed nanoseconds in the provided value

/* usage:
 * double time;
 * std::cout << "start\n";
 * {
 *      Timer<double, std::chrono::seconds> _(time);
 *      std::this_thread::sleep_for(std::chrono::milliseconds(1500));
 * }
 * std::cout << "num secs\t" << time << '\n';
 *
 */
template<typename T, typename Dur>
class Timer
{
public:

        static_assert(std::is_arithmetic_v<T>);
        typedef std::chrono::time_point<std::chrono::steady_clock> ns_clock_type;

        explicit Timer(T &bind_to)
        : bind(bind_to)
        {
                start = std::chrono::steady_clock::now();
        }

        ~Timer()
        {
                std::chrono::nanoseconds dur = std::chrono::steady_clock::now() - start;

                // same duration as what the user provided,
                // but uses the T datatype instead
                typedef std::chrono::duration<T, typename Dur::period> Duration;

                // and we cast the timed results to this datatype
                // and give to the user
                bind = std::chrono::duration_cast<Duration>(dur).count();
        }
private:

        T &bind;
        ns_clock_type start;
};

constexpr
double ns_to_secs(size_t ns)
{
        return static_cast<double>(ns) * .000000001;
}

#endif //BOT_GEN_DEFS_H
