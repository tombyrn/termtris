#ifndef TETRIS
#define TETRIS

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

#define BOARD_WIDTH 20
#define BOARD_HEIGHT 25
#define BORDER_COLOR "\e[1;34m"
#define RESET_SCREEN printf("\x1b[2J\x1b[H")

#define HBLK "\e[0;90m"
#define HRED "\e[0;91m"
#define HGRN "\e[0;92m"
#define HYEL "\e[0;93m"
#define HBLU "\e[0;94m"
#define HMAG "\e[0;95m"
#define HCYN "\e[0;96m"
#define HWHT "\e[0;97m"

#define MAX_BRICKS 4
#define MAX_CLIENTS 4
#define L_PIECE 0
#define I_PIECE 1

extern _Atomic bool alive;
extern char input[3];



#define CHECK(func)                                \
    do {                                           \
        int status = (func);                      \
        if (status < 0) {                        \
            fprintf(stderr, "Error %d at %s:%d\n", \
                    status, __FILE__, __LINE__);  \
            alive = false;                         \
        }                                          \
    } while(0);

// [rotation][brick][x/y]
extern int I_SHAPE_ROTATION[4][2][2];

extern int L_SHAPE_ROTATION[4][3][2];


typedef struct brick {
    char symbol;
    int x, y;
} brick;

typedef struct tetronimo {
    int variation;
    int rotation;
    int x, y;

    bool dead;

    brick bricks[MAX_BRICKS];
    int brick_count;

    char* color;
} tetronimo;

typedef struct tetronimo_array {
    int capacity;
    int size;
    tetronimo* data;
} tetronimo_array;


typedef struct pixel {
    char symbol;
    char color[8];
} pixel;


typedef enum direction {
    NONE,
    LEFT,
    RIGHT,
    DOWN,
    ROTATE
} direction;

typedef struct game_state {
    pixel board[BOARD_HEIGHT][BOARD_WIDTH];
    tetronimo_array block_array;
    direction dir;
    int score;
    bool alive;

} game_state;



void update_bricks(tetronimo* t);
void move_block(tetronimo* block, direction dir, game_state* gs);
void initialize_block(game_state* gs);
void run_server(char* portNo);
void run_client(char* ipNo);
void set_nonblocking(int fd);
void initialize_board(pixel board[BOARD_HEIGHT][BOARD_WIDTH]);
void initialize_block(game_state* gs);
void iterate(game_state* gs);
void move_block(tetronimo* block, direction dir, game_state* gs);
tetronimo* current_block(game_state* gs);
void draw_blocks_to_board(game_state* gs);
#endif