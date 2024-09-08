#include "board.h"

#include <stdio.h>
#include <stdlib.h>

Board* createBoard(int id) {
    Board* board = malloc(sizeof(Board));
    // TODO: Initialize the board with the given id
    int rows = BOARD_SIZE;
    int cols = BOARD_SIZE;
    board->cells = malloc(rows * sizeof(uint8_t*));
    for (int i = 0; i < rows; i++) {
        board->cells[i] = malloc(cols * sizeof(uint8_t));
        for (int j = 0; j < cols; j++) {
            board->cells[i][j] = CELL_EMPTY;
        }
    }

    board->cells[1][1] = CELL_ROCK;
    board->cells[1][8] = CELL_ROCK;
    board->cells[8][1] = CELL_ROCK;
    board->cells[8][8] = CELL_ROCK;
    board->cells[3][3] = CELL_ROCK;
    board->cells[3][6] = CELL_ROCK;
    board->cells[6][3] = CELL_ROCK;
    board->cells[6][6] = CELL_ROCK;

    board->cells[1][4] = CELL_BRICK;
    board->cells[1][5] = CELL_BRICK;
    board->cells[2][2] = CELL_BRICK;
    board->cells[2][7] = CELL_BRICK;
    board->cells[4][1] = CELL_BRICK;
    board->cells[4][4] = CELL_BRICK;
    board->cells[4][5] = CELL_BRICK;
    board->cells[4][8] = CELL_BRICK;
    board->cells[5][1] = CELL_BRICK;
    board->cells[5][4] = CELL_BRICK;
    board->cells[5][5] = CELL_BRICK;
    board->cells[5][8] = CELL_BRICK;
    board->cells[7][2] = CELL_BRICK;
    board->cells[7][7] = CELL_BRICK;
    board->cells[8][4] = CELL_BRICK;
    board->cells[8][5] = CELL_BRICK;

    return board;
}

void destroyBoard(Board* board) {
    int rows = BOARD_SIZE;
    for (int i = 0; i < rows; i++) {
        free(board->cells[i]);
    }
    free(board);
    board = NULL;
}

void printBoard(Board* board) {
    int rows = BOARD_SIZE;
    int cols = BOARD_SIZE;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", board->cells[i][j]);
        }
        printf("\n");
    }
}

void updateBoard(Board* board, int row, int col, uint8_t type) {
    board->cells[row][col] = type;
}

void copyBoard(Board* dest, Board* src) {
    int rows = BOARD_SIZE;
    int cols = BOARD_SIZE;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            dest->cells[i][j] = src->cells[i][j];
        }
    }
}

uint8_t* getDiff(Board* oldBoard, Board* newBoard, uint8_t* nb) {
    int rows = BOARD_SIZE;
    int cols = BOARD_SIZE;
    uint8_t* diff = malloc(rows * cols * sizeof(uint8_t));
    int index = 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (oldBoard->cells[i][j] != newBoard->cells[i][j]) {
                diff[index++] = i;
                diff[index++] = j;
                diff[index++] = newBoard->cells[i][j];
            }
        }
    }
    *nb = index;
    return diff;
}