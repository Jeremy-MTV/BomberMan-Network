#ifndef GAME_H
#define GAME_H

#include "structs.h"
#include "player.h"
#include "board.h"

Game* createGame(GameType type);
void destroyGame(Game* game);
void startGame(Game* game);
void finishGame(Game* game);
void processGame(Game* game);
int isValidMove(Game* game, int row, int col);
void checkBomb(Game* game, int row, int col);
void checkEndGame(Game* game, int currentPlayer);

#endif // !GAME_H