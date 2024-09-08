#include "client.h"

#include <stdio.h>
#include <unistd.h>

void initBoard() {
    int rows = BOARD_SIZE;
    int cols = BOARD_SIZE;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            board[i][j] = CELL_EMPTY;
        }
    }

    {
        // For testing board full

        board[1][1] = CELL_ROCK;
        board[1][8] = CELL_ROCK;
        board[8][1] = CELL_ROCK;
        board[8][8] = CELL_ROCK;
        board[3][3] = CELL_ROCK;
        board[3][6] = CELL_ROCK;
        board[6][3] = CELL_ROCK;
        board[6][6] = CELL_ROCK;
        
        board[1][4] = CELL_BRICK;
        board[1][5] = CELL_BRICK;
        board[2][2] = CELL_BRICK;
        board[2][7] = CELL_BRICK;
        board[4][1] = CELL_BRICK;
        board[4][4] = CELL_BRICK;
        board[4][5] = CELL_BRICK;
        board[4][8] = CELL_BRICK;
        board[5][1] = CELL_BRICK;
        board[5][4] = CELL_BRICK;
        board[5][5] = CELL_BRICK;
        board[5][8] = CELL_BRICK;
        board[7][2] = CELL_BRICK;
        board[7][7] = CELL_BRICK;
        board[8][4] = CELL_BRICK;
        board[8][5] = CELL_BRICK;
        
        board[0][0] = CELL_PLAYER0;
        board[0][9] = CELL_PLAYER1;
        board[9][9] = CELL_PLAYER2;
        board[9][0] = CELL_PLAYER3;
        //
        board[1][2] = CELL_PLAYER0_BOMB;
        board[1][3] = CELL_PLAYER1_BOMB;
        board[2][3] = CELL_PLAYER2_BOMB;
        board[3][2] = CELL_PLAYER3_BOMB;

         board[2][4] = CELL_BOMB;
         board[2][5] = CELL_BOMB;
    }
}

void disconnect(int signum) {
    if (signum == SIGINT || signum == SIGKILL) {
        exit(0);
    }
}

int initTcpClient(const char* address, int port) {
    struct sockaddr_in serverAddr;
    int sockFd;

    if ((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        return -1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(address);
    serverAddr.sin_port = htons(port);

    if (connect(sockFd, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("connection failed");
        return -1;
    }

    return sockFd;
}

void* handleTcpMessage(void* arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        ssize_t receive = recv(tcpSockFd, buffer, BUFFER_SIZE, 0);
        if (receive > 0) {
            processServerMessage(buffer, receive);
        } else if (receive == 0) {
            break;
        } else {
            // Do nothing
        }
        memset(buffer, 0, sizeof(buffer));
    }

    pthread_detach(pthread_self());
    return NULL;
}

void processServerMessage(const char* buffer, long length) {
    // buffer to hex string
    char hex[2 * length + 1];
    for (int i = 0; i < length; i++) {
        sprintf(hex + 2 * i, "%02x", buffer[i] & 0xFF);
    }
    hex[2 * length] = '\0';

    wclear(bottomBar);
    wmove(bottomBar, 1, 1);
    wprintw(bottomBar, "%s: %s", "> TCPServer message", hex);
    wrefresh(bottomBar);

    if (length < 2) {
        return;
    }

    uint16_t responseCodePacket = (buffer[0] << 8) | buffer[1];
    uint16_t responseCode = responseCodePacket >> 3;
    uint8_t responseId = responseCodePacket & 0x06;
    uint8_t responseTeam = responseCodePacket & 0x01;

    if (responseCode == RESPONSE_START_GAME_SOLO || responseCode == RESPONSE_START_GAME_TEAM) {
        if (mode != 0 || length < 22) {
            return;
        }

        mode = responseCode == RESPONSE_START_GAME_SOLO ? 1 : 2;
        id = responseId;
        team = responseTeam;
        processStartGame(buffer, length);
    }
}

void processStartGame(const char* buffer, long length) {
    uint16_t portUdp = (buffer[2] << 8) | buffer[3];
    uint16_t portMDiff = (buffer[4] << 8) | buffer[5];
    struct in6_addr address;
    memcpy(&address, buffer + 6, 16);

    ServerAddress = address;
    ServerPort = portUdp;

    char ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &address, ip, INET6_ADDRSTRLEN);

    wclear(bottomBar);
    wmove(bottomBar, 1, 1);
    wprintw(bottomBar, "%s %s %d %d", "> Start", ip, portUdp, portMDiff);
    wrefresh(bottomBar);

    // Create UDP socket
    udpSockFd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udpSockFd < 0) {
        wclear(bottomBar);
        wmove(bottomBar, 1, 1);
        wprintw(bottomBar, "%s", "> Create UDP socket failed");
        wrefresh(bottomBar);

        mode = 0;
        id = -1;
        team = -1;
        return;
    }

    struct sockaddr_in6 clientAddress;
    clientAddress.sin6_family = AF_INET6;
    clientAddress.sin6_addr = in6addr_any;
    clientAddress.sin6_port = htons(portMDiff);

    int option = 1;
    if (setsockopt(udpSockFd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*) &option, sizeof(option)) < 0) {
        perror("ERROR: setsockopt failed");
    }

    if (bind(udpSockFd, (const struct sockaddr*) &clientAddress, sizeof(clientAddress)) < 0) {
        wclear(bottomBar);
        wmove(bottomBar, 1, 1);
        wprintw(bottomBar, "%s %d", "> UDP Bind failed", portMDiff);
        wrefresh(bottomBar);

        mode = 0;
        id = -1;
        team = -1;
        return;
    }

    struct sockaddr_in6 serverAddress;
    serverAddress.sin6_family = AF_INET6;
    serverAddress.sin6_port = htons(portUdp);
    serverAddress.sin6_addr = in6addr_any;

    char message[2];
    message[0] = 0x22;
    message[1] = 0x33;
    sendto(udpSockFd, message, 2, 0, (const struct sockaddr*) &serverAddress, sizeof(serverAddress));

    pthread_t udpThread;
    if (pthread_create(&udpThread, NULL, (void*) handleUdpMessage, &udpSockFd) != 0) {
        wclear(bottomBar);
        wmove(bottomBar, 1, 1);
        wprintw(bottomBar, "%s", "> Create thread failed");
        wrefresh(bottomBar);

        mode = 0;
        id = -1;
        team = -1;
        return;
    }

    if (mode != 1 && mode != 2) {
        return;
    }

    sendReadyGame();
    runGame();
}

void* handleUdpMessage(void* arg) {
    struct sockaddr_in6 serverAddr;

    int sockFd = *((int*) arg);

    isUdpRunning = 1;
    char buffer[BUFFER_SIZE];
    while (isUdpRunning == 1) {
        socklen_t len = sizeof(serverAddr);
        ssize_t receive = recvfrom(sockFd, (char*) buffer, BUFFER_SIZE, 0, (struct sockaddr*) &serverAddr, &len);

        wclear(bottomBar);
        wmove(bottomBar, 1, 1);
        wprintw(bottomBar, "%s", "> Receive UDP message");
        wrefresh(bottomBar);

        if (receive > 0) {
            processUdpMessage(buffer, receive);
        } else if (receive == 0) {
            break;
        } else {
            // Do nothing
        }
        memset(buffer, 0, sizeof(buffer));

        sleep(1);
    }

    pthread_detach(pthread_self());
    return NULL;
}

void processUdpMessage(const char* buffer, long length) {
    wclear(bottomBar);
    wmove(bottomBar, 1, 1);
    wprintw(bottomBar, "%s", "> Server UDP message");
    wrefresh(bottomBar);

    if (length < 2) {
        return;
    }

    uint16_t responseCodePacket = (buffer[0] << 8) | buffer[1];
    uint16_t responseCode = responseCodePacket >> 3;
    uint8_t responseId = responseCodePacket & 0x06;
    uint8_t responseTeam = responseCodePacket & 0x01;

    if (responseCode == RESPONSE_BOARD_FULL) {
        if (length < 6) {
            return;
        }

        wclear(bottomBar);
        wmove(bottomBar, 1, 1);
        wprintw(bottomBar, "%s", "> Server: update board full");
        wrefresh(bottomBar);

        uint16_t num = (buffer[2] << 8) | buffer[3];
        if (num <= lastUpdateFullNum) {
            return;
        }

        uint8_t rows = buffer[4];
        uint8_t cols = buffer[5];
        if (rows != BOARD_SIZE || cols != BOARD_SIZE) {
            return;
        }

        if (length < 6 + rows * cols) {
            return;
        }

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                board[i][j] = buffer[6 + i * cols + j];
            }
        }
        lastUpdateFullNum = num;
        showBoard();
    } else if (responseCode == RESPONSE_BOARD_CHANGE) {
        if (length < 6) {
            return;
        }

        wclear(bottomBar);
        wmove(bottomBar, 1, 1);
        wprintw(bottomBar, "%s", "> Server: update board change");
        wrefresh(bottomBar);

        uint16_t num = (buffer[2] << 8) | buffer[3];
        if (num <= lastUpdateChangeNum) {
            return;
        }

        uint8_t nb = buffer[4];
        if (nb == 0) {
            lastUpdateChangeNum = num;
            return;
        }

        if (length < 6 + nb * 3) {
            return;
        }

        for (int i = 0; i < nb; i++) {
            uint8_t row = buffer[6 + i * 3];
            uint8_t col = buffer[6 + i * 3 + 1];
            uint8_t cell = buffer[6 + i * 3 + 2];
            board[row][col] = cell;
        }

        lastUpdateChangeNum = num;
        showBoard();
    }
}

void start() {
    int isNotExit = 1;

    char* menus[] = {"SOLO MODE", "TEAM MODE", "EXIT"};
    int mainCursor = 0, menuNum = 3;
    initscr();

    if (has_colors() == FALSE) {
        printf("Your terminal does not support color\n");
        endwin();
        return;
    } else {
        start_color();

        init_pair(COLOR_PAIR_DEFAULT, COLOR_BLACK, COLOR_WHITE);
        init_pair(COLOR_PAIR_BOTTOM_BAR, COLOR_WHITE, COLOR_BLUE);
        init_pair(COLOR_PAIR_TITLE, COLOR_RED, COLOR_WHITE);
        init_pair(COLOR_PAIR_SIDE_BAR, COLOR_BLACK, COLOR_WHITE);

        init_pair(COLOR_PAIR_ROCK, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PAIR_BRICK, COLOR_WHITE, COLOR_CYAN);
        init_pair(COLOR_PAIR_BOMB, COLOR_WHITE, COLOR_RED);
        init_pair(COLOR_PAIR_EXPLOSION, COLOR_WHITE, COLOR_RED);
        init_pair(COLOR_PAIR_PLAYER0, COLOR_BLUE, COLOR_WHITE);
        init_pair(COLOR_PAIR_PLAYER1, COLOR_MAGENTA, COLOR_WHITE);
        init_pair(COLOR_PAIR_PLAYER2, COLOR_GREEN, COLOR_WHITE);
        init_pair(COLOR_PAIR_PLAYER3, COLOR_YELLOW, COLOR_WHITE);
        init_pair(COLOR_PAIR_PLAYER0_BOMB, COLOR_BLUE, COLOR_RED);
        init_pair(COLOR_PAIR_PLAYER1_BOMB, COLOR_MAGENTA, COLOR_RED);
        init_pair(COLOR_PAIR_PLAYER2_BOMB, COLOR_GREEN, COLOR_RED);
        init_pair(COLOR_PAIR_PLAYER3_BOMB, COLOR_YELLOW, COLOR_RED);
    }

    refresh();

    mainWin = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, 0, 0);
    bottomBar = newwin(BOTTOM_BAR_HEIGHT, BOTTOM_BAR_WIDTH, WINDOW_HEIGHT, 0);

    wbkgd(mainWin, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    wbkgd(bottomBar, COLOR_PAIR(COLOR_PAIR_BOTTOM_BAR));

    signal(SIGINT, disconnect);
    signal(SIGKILL, disconnect);

    noecho();
    cbreak();
    curs_set(0);

    keypad(stdscr, TRUE);

    showMainWindow(menus, mainCursor, menuNum);
    int ch;
    while (isNotExit != 0) {
        if (currentState != 0) {
            continue;
        }

        showMainWindow(menus, mainCursor, menuNum);
        wrefresh(bottomBar);
        ch = getch();

        switch (ch) {
            case KEY_UP:
                if (mainCursor != 0) {
                    mainCursor--;
                }
                break;

            case KEY_DOWN:
                if (mainCursor != (menuNum - 1)) {
                    mainCursor++;
                }
                break;

            case '\n':
                switch (mainCursor) {
                    case 0:
                        wclear(bottomBar);
                        wmove(bottomBar, 1, 1);
                        wprintw(bottomBar, "%s", "> Join solo mode");
                        wrefresh(bottomBar);
                        sendJoinGame(REQUEST_JOIN_SOLO);
                        break;

                    case 1:
                        wclear(bottomBar);
                        wmove(bottomBar, 1, 1);
                        wprintw(bottomBar, "%s", "> Join team mode");
                        wrefresh(bottomBar);
                        sendJoinGame(REQUEST_JOIN_TEAM);
                        break;

                    case 2:
                        isNotExit = 0;
                        break;

                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }

    disconnect(0);
    endwin();
}

void showMainWindow(char* menus[], int cursor, int menuNum) {
    int i;
    wclear(mainWin);

    wmove(mainWin, 2, (WINDOW_WIDTH - 14) / 2);
    wattron(mainWin, COLOR_PAIR(COLOR_PAIR_TITLE));
    wprintw(mainWin, "%s", "Bomberman Game");
    wattroff(mainWin, COLOR_PAIR(COLOR_PAIR_TITLE));

    int startRow = (WINDOW_HEIGHT - (menuNum * 2 + 1)) / 2;

    for (i = 0; i < menuNum; i++) {
        int col = (int) (WINDOW_WIDTH - strlen(menus[i])) / 2;
        if (cursor == i) {
            wattron(mainWin, A_REVERSE);
            wmove(mainWin, startRow + i * 2, col);
            wprintw(mainWin, "%s", menus[i]);
            wattroff(mainWin, A_REVERSE);
        } else {
            wmove(mainWin, startRow + i * 2, col);
            wprintw(mainWin, "%s", menus[i]);
        }
    }

    wrefresh(mainWin);
}

void sendJoinGame(int codereq) {
    uint16_t requestPacket = codereq << 3;
    char* request = (char*) malloc(2);
    request[0] = (char) ((requestPacket >> 8) & 0xFF);
    request[1] = (char) (requestPacket & 0xFF);
    send(tcpSockFd, request, 2, 0);
    free(request);

    wclear(bottomBar);
    wmove(bottomBar, 1, 1);
    wprintw(bottomBar, "%s", "> Waiting for server response");
    wrefresh(bottomBar);
}

void sendReadyGame() {
    if (mode != 1 && mode != 2) {
        return;
    }

    uint16_t requestPacket = mode == 1 ? (REQUEST_READY_SOLO << 3) : (REQUEST_READY_TEAM << 3);
    requestPacket |= id << 1;
    requestPacket |= team;

    char* request = (char*) malloc(2);
    request[0] = (char) ((requestPacket >> 8) & 0xFF);
    request[1] = (char) (requestPacket & 0xFF);
    send(tcpSockFd, request, 2, 0);
    free(request);

    // wclear(bottomBar);
    // wmove(bottomBar, 1, 1);
    // wprintw(bottomBar, "%s", "> Ready game");
    // wrefresh(bottomBar);
}

void runGame() {
    initBoard();

    currentState = 1;
    isRunning = 1;
    actionCount = 0;
    lastUpdateFullNum = -1;
    lastUpdateChangeNum = -1;

    gameWin = newwin(GAME_WIN_HEIGHT, GAME_WIN_WIDTH, 0, 0);
    gameSideBar = newwin(GAME_SIDE_BAR_HEIGHT, GAME_SIDE_BAR_WIDTH, 0, GAME_WIN_WIDTH);

    wbkgd(gameWin, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    wbkgd(gameSideBar, COLOR_PAIR(COLOR_PAIR_SIDE_BAR));

    int key;
    while (isRunning == 1) {
        showBoard();
        showSideBar();
        wrefresh(gameSideBar);
        wrefresh(bottomBar);

        key = getch();
        switch (key) {
            case KEY_UP:
            case 'w':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> UP");
                wrefresh(bottomBar);
                sendAction(ACTION_MOVE_NORTH);
                break;

            case KEY_DOWN:
            case 's':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> DOWN");
                wrefresh(bottomBar);
                sendAction(ACTION_MOVE_SOUTH);
                break;

            case KEY_LEFT:
            case 'a':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> LEFT");
                wrefresh(bottomBar);
                sendAction(ACTION_MOVE_WEST);
                break;

            case KEY_RIGHT:
            case 'd':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> RIGHT");
                wrefresh(bottomBar);
                sendAction(ACTION_MOVE_EAST);
                break;

            case ' ':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> BOMB");
                wrefresh(bottomBar);
                sendAction(ACTION_BOMB);
                break;

            case 'q':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> QUIT");
                wrefresh(bottomBar);
                // isRunning = 0;
                break;

            case 'r':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> CHAT ALL");
                wrefresh(bottomBar);
                break;

            case 't':
                wclear(bottomBar);
                wmove(bottomBar, 1, 1);
                wprintw(bottomBar, "%s", "> CHAT TEAM");
                wrefresh(bottomBar);
                break;

            default:
                break;
        }
    }

    delwin(gameWin);
    delwin(gameSideBar);
    currentState = 0;
}

void showBoard() {
    wclear(gameWin);

    int rows = BOARD_SIZE;
    int cols = BOARD_SIZE;

    int startRow = (GAME_WIN_HEIGHT - rows * 2 - 1) / 2;
    int startCol = (GAME_WIN_WIDTH - cols * 4 - 1) / 2;
    int endRow = startRow + rows * 2;
    int endCol = startCol + cols * 4;

    // Draw board
    {
        for (int r = startRow; r <= endRow; r += 2) {
            for (int c = startCol; c <= endCol; c++) {
                mvwaddch(gameWin, r, c, ACS_HLINE);
            }
        }

        for (int r = startRow; r <= endRow; r++) {
            for (int c = startCol; c <= endCol; c += 4) {
                mvwaddch(gameWin, r, c, ACS_VLINE);
            }
        }

        mvwaddch(gameWin, startRow, startCol, ACS_ULCORNER);
        mvwaddch(gameWin, endRow, startCol, ACS_LLCORNER);
        mvwaddch(gameWin, startRow, endCol, ACS_URCORNER);
        mvwaddch(gameWin, endRow, endCol, ACS_LRCORNER);

        for (int r = startRow + 2; r < endRow; r += 2) {
            mvwaddch(gameWin, r, startCol, ACS_LTEE);
            mvwaddch(gameWin, r, endCol, ACS_RTEE);
            for (int c = startCol + 4; c < endCol; c += 4) {
                mvwaddch(gameWin, r, c, ACS_PLUS);
            }
        }

        for (int c = startCol + 4; c < endCol; c += 4) {
            mvwaddch(gameWin, startRow, c, ACS_TTEE);
            mvwaddch(gameWin, endRow, c, ACS_BTEE);
        }
    }

    // Draw item
    {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                int r = startRow + 1 + i * 2;
                int c = startCol + 1 + j * 4;

                switch (board[i][j]) {
                    case CELL_ROCK:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_ROCK));
                        mvwprintw(gameWin, r, c, ROCK_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_ROCK));
                        break;

                    case CELL_BRICK:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_BRICK));
                        mvwprintw(gameWin, r, c, BRICK_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_BRICK));
                        break;

                    case CELL_BOMB:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_BOMB));
                        mvwprintw(gameWin, r, c, BOMB_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_BOMB));
                        break;

                    case CELL_EXPLOSION:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_EXPLOSION));
                        mvwprintw(gameWin, r, c, EXPLOSION_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_EXPLOSION));
                        break;

                    case CELL_PLAYER0:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER0));
                        mvwprintw(gameWin, r, c, PLAYER0_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER0));
                        break;

                    case CELL_PLAYER1:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER1));
                        mvwprintw(gameWin, r, c, PLAYER1_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER1));
                        break;

                    case CELL_PLAYER2:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER2));
                        mvwprintw(gameWin, r, c, PLAYER2_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER2));
                        break;

                    case CELL_PLAYER3:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER3));
                        mvwprintw(gameWin, r, c, PLAYER3_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER3));
                        break;

                    case CELL_PLAYER0_BOMB:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER0_BOMB));
                        mvwprintw(gameWin, r, c, PLAYER0_BOMB_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER0_BOMB));
                        break;

                    case CELL_PLAYER1_BOMB:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER1_BOMB));
                        mvwprintw(gameWin, r, c, PLAYER1_BOMB_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER1_BOMB));
                        break;

                    case CELL_PLAYER2_BOMB:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER2_BOMB));
                        mvwprintw(gameWin, r, c, PLAYER2_BOMB_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER2_BOMB));
                        break;

                    case CELL_PLAYER3_BOMB:
                        wattron(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER3_BOMB));
                        mvwprintw(gameWin, r, c, PLAYER3_BOMB_CHAR);
                        wattroff(gameWin, COLOR_PAIR(COLOR_PAIR_PLAYER3_BOMB));
                        break;

                    default:
                        break;
                }
            }
        }
    }

    wrefresh(gameWin);
}

void showSideBar() {
    wclear(gameSideBar);

    if (mode == 1) {
        wmove(gameSideBar, 1, 1);
        wprintw(gameSideBar, "%s", "SOLO MODE");
    } else if (mode == 2) {
        wmove(gameSideBar, 1, 1);
        wprintw(gameSideBar, "%s", "TEAM MODE");
    }

    wrefresh(gameSideBar);
}

void sendAction(int code) {
    if (mode != 1 && mode != 2) {
        return;
    }

    uint16_t requestPacket = mode == 1 ? (REQUEST_ACTION_SOLO << 3) : (REQUEST_ACTION_TEAM << 3);
    requestPacket |= id << 1;
    requestPacket |= team;

    uint16_t actionData = 0;
    actionData |= actionCount << 3;
    actionData |= code;

    char* message = (char*) malloc(4);
    message[0] = (char) ((requestPacket >> 8) & 0xFF);
    message[1] = (char) (requestPacket & 0xFF);
    message[2] = (char) ((actionData >> 8) & 0xFF);
    message[3] = (char) (actionData & 0xFF);

    struct sockaddr_in6 serverAddress;
    serverAddress.sin6_family = AF_INET6;
    serverAddress.sin6_port = htons(ServerPort);
    serverAddress.sin6_addr = ServerAddress;

    sendto(udpSockFd, message, 4, 0, (struct sockaddr*) &serverAddress, sizeof(serverAddress));

    free(message);

    actionCount = (actionCount + 1) % MAX_NUM_ACTIONS;
}

int main(int argc, char** argv) {
    tcpSockFd = initTcpClient(SERVER_ADDRESS4, SERVER_PORT);
    if (tcpSockFd < 0) {
        return EXIT_FAILURE;
    }

    pthread_t tcpThread;
    if (pthread_create(&tcpThread, NULL, (void*) handleTcpMessage, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    start();

    return 0;
}