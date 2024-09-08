#include "game.h"
#include "utils.h"

#include <stdlib.h>

Game* createGame(GameType type) {
    if (type != GAME_TYPE_SOLO && type != GAME_TYPE_TEAM) {
        type = GAME_TYPE_SOLO;
    }

    Game* game = (Game*) malloc(sizeof(Game));
    game->type = type;
    for (int i = 0; i < NUM_PLAYERS; i++) {
        game->players[i] = NULL;
        game->actionsMove[i] = NULL;
        game->actionsBomb[i] = NULL;
    }
    game->board = createBoard(0);
    game->status = GAME_STATUS_WAITING;
    game->winner = -1;
    return game;
}

void destroyGame(Game* game) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (game->players[i] != NULL) {
            destroyPlayer(game->players[i]);
        }
    }
    destroyBoard(game->board);
    free(game);
    game = NULL;
}

void startGame(Game* game) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (game->players[i] == NULL || game->players[i]->status != PLAYER_STATUS_READY) {
            return;
        }
    }

    game->players[0]->row = 0;
    game->players[0]->col = 0;
    updateBoard(game->board, 0, 0, CELL_PLAYER0);

    game->players[1]->row = 0;
    game->players[1]->col = BOARD_SIZE - 1;
    updateBoard(game->board, 0, BOARD_SIZE - 1, CELL_PLAYER1);

    game->players[2]->row = BOARD_SIZE - 1;
    game->players[2]->col = BOARD_SIZE - 1;
    updateBoard(game->board, BOARD_SIZE - 1, BOARD_SIZE - 1, CELL_PLAYER2);

    game->players[3]->row = BOARD_SIZE - 1;
    game->players[3]->col = 0;
    updateBoard(game->board, BOARD_SIZE - 1, 0, CELL_PLAYER3);

    for (int i = 0; i < NUM_PLAYERS; i++) {
        game->players[i]->status = PLAYER_STATUS_PLAYING;
    }
    game->status = GAME_STATUS_PLAYING;
}

void finishGame(Game* game) {
    game->status = GAME_STATUS_FINISHED;
}

int isValidMove(Game* game, int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return 0;
    }

    return game->board->cells[row][col] == CELL_EMPTY;
}

void checkBomb(Game* game, int row, int col) {
    // Check current cell
    if (game->board->cells[row][col] == CELL_BOMB) {
        updateBoard(game->board, row, col, CELL_EMPTY);
    }
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (game->players[i]->row == row && game->players[i]->col == col) {
            killPlayer(game->players[i]);
        }
    }

    // Check north with 2 cells
    for (int i = 1; i <= 2; i++) {
        if (game->board->cells[row - i][col] == CELL_BOMB) {
            break;
        }

        if (row - i < 0) {
            break;
        }

        if (game->board->cells[row - i][col] == CELL_ROCK) {
            break;
        }

        if (game->board->cells[row - i][col] == CELL_BRICK) {
            updateBoard(game->board, row - i, col, CELL_EMPTY);
            break;
        }

        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (game->players[j]->row == row - i && game->players[j]->col == col) {
                updateBoard(game->board, row - i, col, CELL_EMPTY);
                killPlayer(game->players[j]);
            }
        }
    }

    // Check east with 2 cells
    for (int i = 1; i <= 2; i++) {
        if (game->board->cells[row][col + i] == CELL_BOMB) {
            break;
        }

        if (col + i >= BOARD_SIZE) {
            break;
        }

        if (game->board->cells[row][col + i] == CELL_ROCK) {
            break;
        }

        if (game->board->cells[row][col + i] == CELL_BRICK) {
            updateBoard(game->board, row, col + i, CELL_EMPTY);
            break;
        }

        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (game->players[j]->row == row && game->players[j]->col == col + i) {
                updateBoard(game->board, row, col + i, CELL_EMPTY);
                killPlayer(game->players[j]);
            }
        }
    }

    // Check south with 2 cells
    for (int i = 1; i <= 2; i++) {
        if (game->board->cells[row + i][col] == CELL_BOMB) {
            break;
        }

        if (row + i >= BOARD_SIZE) {
            break;
        }

        if (game->board->cells[row + i][col] == CELL_ROCK) {
            break;
        }

        if (game->board->cells[row + i][col] == CELL_BRICK) {
            updateBoard(game->board, row + i, col, CELL_EMPTY);
            break;
        }

        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (game->players[j]->row == row + i && game->players[j]->col == col) {
                updateBoard(game->board, row + i, col, CELL_EMPTY);
                killPlayer(game->players[j]);
            }
        }
    }

    // Check west with 2 cells
    for (int i = 1; i <= 2; i++) {
        if (game->board->cells[row][col - i] == CELL_BOMB) {
            break;
        }

        if (col - i < 0) {
            break;
        }

        if (game->board->cells[row][col - i] == CELL_ROCK) {
            break;
        }

        if (game->board->cells[row][col - i] == CELL_BRICK) {
            updateBoard(game->board, row, col - i, CELL_EMPTY);
            break;
        }

        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (game->players[j]->row == row && game->players[j]->col == col - i) {
                updateBoard(game->board, row, col - i, CELL_EMPTY);
                killPlayer(game->players[j]);
            }
        }
    }

    // Check north-east with 1 cell
    if (row - 1 >= 0 && col + 1 < BOARD_SIZE) {
        if (game->board->cells[row - 1][col + 1] == CELL_BOMB || game->board->cells[row - 1][col + 1] == CELL_ROCK) {
            // Ignore
        } else if (game->board->cells[row - 1][col + 1] == CELL_BRICK) {
            updateBoard(game->board, row - 1, col + 1, CELL_EMPTY);
        } else {
            for (int j = 0; j < NUM_PLAYERS; j++) {
                if (game->players[j]->row == row - 1 && game->players[j]->col == col + 1) {
                    updateBoard(game->board, row - 1, col + 1, CELL_EMPTY);
                    killPlayer(game->players[j]);
                }
            }
        }
    }

    // Check south-east with 1 cell
    if (row + 1 < BOARD_SIZE && col + 1 < BOARD_SIZE) {
        if (game->board->cells[row + 1][col + 1] == CELL_BOMB || game->board->cells[row + 1][col + 1] == CELL_ROCK) {
            // Ignore
        } else if (game->board->cells[row + 1][col + 1] == CELL_BRICK) {
            updateBoard(game->board, row + 1, col + 1, CELL_EMPTY);
        } else {
            for (int j = 0; j < NUM_PLAYERS; j++) {
                if (game->players[j]->row == row + 1 && game->players[j]->col == col + 1) {
                    updateBoard(game->board, row + 1, col + 1, CELL_EMPTY);
                    killPlayer(game->players[j]);
                }
            }
        }
    }

    // Check south-west with 1 cell
    if (row + 1 < BOARD_SIZE && col - 1 >= 0) {
        if (game->board->cells[row + 1][col - 1] == CELL_BOMB || game->board->cells[row + 1][col - 1] == CELL_ROCK) {
            // Ignore
        } else if (game->board->cells[row + 1][col - 1] == CELL_BRICK) {
            updateBoard(game->board, row + 1, col - 1, CELL_EMPTY);
        } else {
            for (int j = 0; j < NUM_PLAYERS; j++) {
                if (game->players[j]->row == row + 1 && game->players[j]->col == col - 1) {
                    updateBoard(game->board, row + 1, col - 1, CELL_EMPTY);
                    killPlayer(game->players[j]);
                }
            }
        }
    }

    // Check north-west with 1 cell
    if (row - 1 >= 0 && col - 1 >= 0) {
        if (game->board->cells[row - 1][col - 1] == CELL_BOMB || game->board->cells[row - 1][col - 1] == CELL_ROCK) {
            // Ignore
        } else if (game->board->cells[row - 1][col - 1] == CELL_BRICK) {
            updateBoard(game->board, row - 1, col - 1, CELL_EMPTY);
        } else {
            for (int j = 0; j < NUM_PLAYERS; j++) {
                if (game->players[j]->row == row - 1 && game->players[j]->col == col - 1) {
                    updateBoard(game->board, row - 1, col - 1, CELL_EMPTY);
                    killPlayer(game->players[j]);
                }
            }
        }
    }
}

void checkEndGame(Game* game, int currentPlayer) {
    int numPlayers = 0;
    int winner = -1;
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (game->players[i]->status == PLAYER_STATUS_PLAYING) {
            numPlayers++;
            winner = i;
        }
    }

    if (numPlayers == 1) {
        game->winner = game->type == GAME_TYPE_SOLO ? winner : game->players[winner]->team;
        finishGame(game);
    } else if (numPlayers == 0) {
        game->winner = game->type == GAME_TYPE_SOLO ? currentPlayer : game->players[currentPlayer]->team;
        finishGame(game);
    } else if (numPlayers == 2 && game->type == GAME_TYPE_TEAM) {
        if (game->players[0]->status == PLAYER_STATUS_PLAYING && game->players[2]->status == PLAYER_STATUS_PLAYING) {
            game->winner = game->players[0]->team;
            finishGame(game);
        } else if (game->players[1]->status == PLAYER_STATUS_PLAYING && game->players[3]->status == PLAYER_STATUS_PLAYING) {
            game->winner = game->players[1]->team;
            finishGame(game);
        }
    }
}

void processGame(Game* game) {
    if (game->status != GAME_STATUS_PLAYING) {
        return;
    }

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (game->players[i]->status == PLAYER_STATUS_DEAD || game->players[i]->status == PLAYER_STATUS_QUIT) {
            continue;
        }

        int row, col;
        if (game->actionsMove[i] != NULL) {
            switch (game->actionsMove[i]->action) {
                case ACTION_MOVE_NORTH:
                    row = game->players[i]->row - 1;
                    col = game->players[i]->col;
                    if (isValidMove(game, row, col)) {
                        if (game->board->cells[game->players[i]->row][game->players[i]->col] == CELL_PLAYER0_BOMB + i) {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_BOMB);
                        } else {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_EMPTY);
                        }
                        game->players[i]->row = row;
                        updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_PLAYER0 + i);
                    }
                    break;

                case ACTION_MOVE_EAST:
                    row = game->players[i]->row;
                    col = game->players[i]->col + 1;
                    if (isValidMove(game, row, col)) {
                        if (game->board->cells[game->players[i]->row][game->players[i]->col] == CELL_PLAYER0_BOMB + i) {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_BOMB);
                        } else {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_EMPTY);
                        }
                        game->players[i]->col = col;
                        updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_PLAYER0 + i);
                    }
                    break;

                case ACTION_MOVE_SOUTH:
                    row = game->players[i]->row + 1;
                    col = game->players[i]->col;
                    if (isValidMove(game, row, col)) {
                        if (game->board->cells[game->players[i]->row][game->players[i]->col] == CELL_PLAYER0_BOMB + i) {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_BOMB);
                        } else {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_EMPTY);
                        }
                        game->players[i]->row = row;
                        updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_PLAYER0 + i);
                    }
                    break;

                case ACTION_MOVE_WEST:
                    row = game->players[i]->row;
                    col = game->players[i]->col - 1;
                    if (isValidMove(game, row, col)) {
                        if (game->board->cells[game->players[i]->row][game->players[i]->col] == CELL_PLAYER0_BOMB + i) {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_BOMB);
                        } else {
                            updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_EMPTY);
                        }
                        game->players[i]->col = col;
                        updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_PLAYER0 + i);
                    }
                    break;
            }
        }

        if (game->actionsBomb[i] != NULL) {
            if (game->players[i]->bomb == 0) {
                game->players[i]->bomb = getCurrentTimeMillis();
                game->players[i]->bombRow = game->players[i]->row;
                game->players[i]->bombCol = game->players[i]->col;
                updateBoard(game->board, game->players[i]->row, game->players[i]->col, CELL_PLAYER0_BOMB + i);
            }
        }

        if (game->players[i]->bomb != 0 && getCurrentTimeMillis() - game->players[i]->bomb >= BOMB_TIME) {
            game->players[i]->bomb = 0;
            checkBomb(game, game->players[i]->bombRow, game->players[i]->bombCol);
        }

        checkEndGame(game, i);
        if (game->status == GAME_STATUS_FINISHED) {
            break;
        }
    }
}