#ifndef BOT_DEV_EVAL_H
#define BOT_DEV_EVAL_H

#include "position.h"
#include "gen-defs.h"
#include <limits>

// we work with the ply of the mate, not number of "moves" because it is easier
constexpr int max_mate_ply = 256;

typedef int32_t Eval;
constexpr Eval worst_white = std::numeric_limits<int32_t>::lowest();
constexpr Eval worst_black = std::numeric_limits<int32_t>::max();

// white mated in <x> is worst_white + x
// and idem for black
inline auto white_is_mated (Eval eval) -> bool {return eval < worst_white + max_mate_ply;}
inline auto black_is_mated (Eval eval) -> bool {return eval > worst_black - max_mate_ply;}
inline auto is_mate (Eval eval) -> bool {return white_is_mated(eval) || black_is_mated(eval);}
inline auto truncated (Eval eval) -> Eval
{
        // truncates the eval to non-mate
        if (white_is_mated(eval))
                return worst_white + max_mate_ply;
        if (black_is_mated(eval))
                return worst_black - max_mate_ply;
        return eval;
}

// returns true if from the perspective of col
// the left eval is better than the right
// this also works for mate in <n>
template <Color col>
auto is_better_than (Eval left, Eval right) -> bool
{
        if constexpr (col == Color::white)
                return left > right;
        else
                return left < right;
}

template <Color col>
auto is_worse_than (Eval left, Eval right) -> bool
{
        if constexpr (col == Color::white)
                return left < right;
        else
                return left > right;
}

/*
// evaluates the position recursively
constexpr
auto eval_deep (const Position &board, unsigned int layers_beyond) -> Eval;
*/

inline
auto static_eval (const Position &board) -> Eval;


//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
/// IMPLEMENTATION /// IMPLEMENTATION /// IMPLEMENTATION /// IMPLEMENTATION /// IMPLEMENTATION ///
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////


// heuristic constants
// unit centi pawn
constexpr int queen_val = 900;
constexpr int rook_val = 500;
constexpr int bishop_val = 350;
constexpr int horse_val = 300;
constexpr int pawn_val = 100;
constexpr int par_square_attack_score = 4;

// points for this piece being attacked
constexpr int queen_attack_val = 40;
constexpr int rook_attack_val = 30;
constexpr int bishop_attack_val = 20;
constexpr int horse_attack_val = 25;
constexpr int pawn_attack_val = 5;

// we like it when a pawn attacks something strong
constexpr int pawn_delta_attack_val = 10;

// bonus for a rook that doesn't look at a friendly pawn
constexpr int unblocked_rook_score = 10;

constexpr int double_pawn_penalty = 20;
// pawns get a bonus for defending another pawn
constexpr int pawn_chain_defense_bonus = 10;

constexpr int king_safety_lastrank_val = 40;
constexpr int king_safety_2ndlastrank_val = 15;

// todo quiescence seacg?
constexpr int attack_other_king_val = 40;


// heuristic evaluation method
template <Color col>  // col to move
constexpr
auto eval_col (const Position &board) -> Eval
{
        constexpr bool is_white = col == Color::white;

        Eval score = 0;
        const Field atm = board.attack_map<col>();
        const Field all_friendly = board.get_occupation<col>();

        const Field &queens  = board.queen<col>(),
                    &pawns   = board.pawns<col>(),
                    &rooks   = board.rooks<col>(),
                    &horses  = board.horses<col>(),
                    &bishops = board.bishops<col>(),
                    &king    = board.king<col>();

        const Field &enemy_queens  = board.queen<!col>(),
                    &enemy_pawns   = board.pawns<!col>(),
                    &enemy_rooks   = board.rooks<!col>(),
                    &enemy_horses  = board.horses<!col>(),
                    &enemy_bishops = board.bishops<!col>(),
                    &enemy_king    = board.king<!col>();


        const uint8_t num_queens  = bit_count(queens),
                      num_pawns   = bit_count(pawns),
                      num_rooks   = bit_count(rooks),
                      num_horses  = bit_count(horses),
                      num_bishops = bit_count(bishops);

        score  = queen_val  * num_queens;
        score += rook_val   * num_rooks;
        score += horse_val  * num_horses;
        score += bishop_val * num_bishops;
        score += pawn_val   * num_pawns;

        // bonus bishop pair if there are bishops on both colors
        if (bishops & msk::white_squares && bishops & msk::black_squares) {
                score += 40;
        }

        // assign score to "activity" of board
        score += par_square_attack_score * bit_count(atm);      // general activity

        // attacking pieces is good too
        score += queen_attack_val  * bit_count(atm & enemy_queens);
        score += rook_attack_val   * bit_count(atm & enemy_rooks);
        score += bishop_attack_val * bit_count(atm & enemy_bishops);
        score += horse_attack_val  * bit_count(atm & enemy_horses);
        score += pawn_attack_val   * bit_count(atm & enemy_pawns);

        auto rel_rank = [](int r) -> Field {
                return is_white ? msk::rank[r] : msk::rank[7 - r];
        };

        // central pawns are worth more
        score += 15  * bit_count(pawns & ~msk::file[0] & ~msk::file[7]);

        // and rook-file pawns less
        score -= 10 * bit_count(pawns & msk::file[0] & msk::file[7]);

        // pawn promotion
        // score += 1. * bit_count(board.pawns<col>() & rel_rank(5));
        // score += 2. * bit_count(board.pawns<col>() & rel_rank(6));

        score += 100 * bit_count(pawns & rel_rank(5));
        score += 150 * bit_count(pawns & rel_rank(6));

        // horsies get penalty on the edges
        score -= 40 * bit_count(horses & msk::ring[0]);
        score -= 20 * bit_count(horses & msk::ring[1]);
        score += 20 * bit_count(horses & msk::ring[3]);

        // horses get a bonus if there are more pawns
        score += num_horses * 6 * bit_count(pawns | enemy_pawns);

        // bishops get a slight penalty for pawns
        score -= num_bishops * 3 * bit_count(pawns | enemy_pawns);

        // bonus for rooks that don't look at a friendly pawn ahead

        int num_unblocked_rooks = 0;
        int num_double_pawns = 0;
        for (Field file = msk::file[0]; file; shift<east>(file)) {
                if (file & rooks && (file & pawns) == 0ull) {
                        num_unblocked_rooks++;
                }
                const int num_pawns = bit_count(pawns & file);
                if (num_pawns > 1) {
                        num_double_pawns += num_pawns - 1;
                }
        }

        score += unblocked_rook_score * num_unblocked_rooks;
        score -= double_pawn_penalty * num_double_pawns;

        // pawns seeing bigger pieces is a nice bonus
        const Field pawns_east_attack = is_white ? shifted<northEast>(pawns) : shifted<southEast>(pawns);
        const Field pawns_west_attack = is_white ? shifted<northWest>(pawns) : shifted<southWest>(pawns);;

        score += pawn_chain_defense_bonus * bit_count(pawns_east_attack & pawns);
        score += pawn_chain_defense_bonus * bit_count(pawns_west_attack & pawns);

        score += pawn_delta_attack_val * ((rook_val - 100) / 100) * bit_count(pawns_east_attack & enemy_rooks);
        score += pawn_delta_attack_val * ((rook_val - 100) / 100) * bit_count(pawns_west_attack & enemy_rooks);
        score += pawn_delta_attack_val * ((queen_val - 100) / 100) * bit_count(pawns_east_attack & enemy_queens);
        score += pawn_delta_attack_val * ((queen_val - 100) / 100) * bit_count(pawns_west_attack & enemy_queens);
        score += pawn_delta_attack_val * ((horse_val - 100) / 100) * bit_count(pawns_east_attack & enemy_horses);
        score += pawn_delta_attack_val * ((horse_val - 100) / 100) * bit_count(pawns_west_attack & enemy_horses);
        score += pawn_delta_attack_val * ((bishop_val - 100) / 100) * bit_count(pawns_east_attack & enemy_bishops);
        score += pawn_delta_attack_val * ((bishop_val - 100) / 100) * bit_count(pawns_west_attack & enemy_bishops);

        // and king safety
        score += king_safety_2ndlastrank_val * bit_count(king & (msk::rank[1] | msk::rank[6]));
        score += king_safety_2ndlastrank_val * bit_count(king & (msk::file[1] | msk::file[6]));
        score += king_safety_lastrank_val * bit_count(king & (msk::rank[0] | msk::rank[7]));
        score += king_safety_lastrank_val * bit_count(king & (msk::file[0] | msk::file[7]));

        // penalty for open area around king
        score -= 15 * bit_count(get_king_area(OneSquare_unsafe(king)) & ~all_friendly);

        if (atm & enemy_king)
                score += attack_other_king_val;
        
        return score;
}

inline
auto static_eval(const Position &board) -> Eval
{
        return truncated(eval_col<Color::white>(board) - eval_col<Color::black>(board));
}

#endif //BOT_DEV_EVAL_H
