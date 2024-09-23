#ifndef CLI_GAME
#define CLI_GAME

// for command line chess game
// dependency: engine

#include "../Engine/position.h"
#include <optional>
#include <string>

void print(const PiecewiseBoard &board);

struct GameSettings {
        enum {
                white, black, both
        } player_colors;
};

// move format for the cli game
struct CliMove {

};

struct UserInput {

        enum {
                back, flip_board
        } game_set;

};

UserInput askUserInput();



#endif //CLI_GAME
