#ifndef BOARD_H
#define BOARD_H

#include "structs.h"

Board* createBoard(int id);
void destroyBoard(Board* board);
void printBoard(Board* board);
void updateBoard(Board* board, int row, int col, uint8_t type);
void copyBoard(Board* dest, Board* src);
uint8_t* getDiff(Board* oldBoard, Board* newBoard, uint8_t* nb);

#endif // !BOARD_H