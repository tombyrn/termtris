#include "tetris.h"

struct termios orig_termios; // terminal input output struct
char input[3] = {0}; // input buffer

_Atomic bool alive = true;
_Atomic bool rotate = false;

game_state sp_game_state; // single player game state

pthread_t input_t; // separate thread for input processing
pthread_attr_t input_t_attr;
pthread_mutex_t input_mutex;

int I_SHAPE_ROTATION[4][2][2] = {
    // rotation 0
    {
        {0,0},
        {0,1}
    },
    // rotation 1
    {
        {1,0},
        {0,0}
    },
    // rotation 2
    {
        {1,1},
        {1,0}
    },
    // rotation 3
    {
        {0,1},
        {1,1}
    }
};

int L_SHAPE_ROTATION[4][3][2] = {
        // rotation 0
        {
            {0,0},
            // {1,0},
            {0,1},
            {1,1}
        },
        // rotation 1
        {
            {1,0},
            // {1,1},
            {0,0},
            {0,1}
        },
        // rotation 2
        {
            {1,1},
            // {0,1},
            {1,0},
            {0,0}
        },
        // rotation 3
        {
            {0,1},
            // {0,0},
            {1,1},
            {1,0}
        } 
};

// set terminal to raw mode so it can read user input immediately
void enable_raw_input() {
    CHECK(tcgetattr(STDIN_FILENO, &orig_termios) != 0);

    struct termios raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ECHO | ICANON);
    raw_termios.c_cc[VMIN] = 0;
    raw_termios.c_cc[VTIME] = 0;

    CHECK(tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios) != 0);
}

// set terminal back to cooked mode
void disable_raw_input() {
    CHECK(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios));
}

// signal handler function for SIGINT
void sig_int(int sig) {
    alive = false;
}

// cleans up threads, mutex, and heap memory after exit
void cleanup() {
    alive = false;

    CHECK(pthread_join(input_t, NULL));
    CHECK(pthread_mutex_destroy(&input_mutex));

    if(sp_game_state.block_array.data)
        free(sp_game_state.block_array.data);

    disable_raw_input();

    CHECK(pthread_attr_destroy(&input_t_attr));
}

// wipes board every frame so blocks can be drawn to it
void initialize_board(pixel board[BOARD_HEIGHT][BOARD_WIDTH]) {
    for (int i = 0; i < BOARD_HEIGHT; i++) {
        for (int j = 0; j < BOARD_WIDTH; j++) {
            strncpy(board[i][j].color, BORDER_COLOR, 7);
            if(i == BOARD_HEIGHT - 1 || j == 0 || j == BOARD_WIDTH - 1)
                board[i][j].symbol = '#';
            else
                board[i][j].symbol = ' ';
        }
    }
}

// prints the board to the terminal
void draw_board() {
    for (int i = 0; i < BOARD_HEIGHT; i++) {
        for (int j = 0; j < BOARD_WIDTH; j++) {
            printf("%s", sp_game_state.board[i][j].color);
            putchar(sp_game_state.board[i][j].symbol);
        }
        if(i == 1)
            printf("\tSCORE");
        if(i == 2)
            printf("\t%5d", sp_game_state.score);
        putchar('\n');
    }

}

// returns addres of currently falling block
tetronimo* current_block(game_state* gs) {
    tetronimo_array block_array = gs->block_array;
    if(block_array.size == 0)
        return NULL;

    return &block_array.data[block_array.size - 1];
}

// writes the array of tetronimos to the board that gets drawn on screen
void draw_blocks_to_board(game_state* gs) {
    tetronimo_array* block_array = &gs->block_array;
    for(int i = 0; i < block_array->size; i++) {
        tetronimo* current = &block_array->data[i];
        for(int i = 0; i < current->brick_count; i++) {
            brick b = current->bricks[i];
            gs->board[b.y][b.x].symbol = b.symbol;
            strncpy(gs->board[b.y][b.x].color, current->color, 7);
        }
    }
}

// returns random color as ANSI escape code
char* random_color() {
    int rand_color = rand() % 7;
    switch(rand_color) {
        case 0: return HRED;
        case 1: return HGRN;
        case 2: return HYEL;
        case 3: return HBLU;
        case 4: return HMAG;
        case 5: return HCYN;
        case 6: return HWHT;
    }
    return HBLU;
}

// runs only on startup (SINGLE PLAYER)
void setup() {
    sp_game_state.block_array.capacity = 256;
    sp_game_state.block_array.size = 0;
    sp_game_state.block_array.data = calloc(sp_game_state.block_array.capacity, sizeof(tetronimo));
    sp_game_state.score = 0;
    if(sp_game_state.block_array.data == NULL) {
        printf("block array cannot be allocated\n");
        exit(1);
    }
    initialize_block(&sp_game_state);
}

// creates new tetronimo block for the user to control
void initialize_block(game_state* gs) {
    int i = gs->block_array.size;
    tetronimo_array* block_array = &gs->block_array;

    block_array->data[i].dead = false;
    block_array->data[i].variation = rand() % 2;
    block_array->data[i].rotation = 0;
    
    block_array->data[i].x = BOARD_WIDTH / 2;
    block_array->data[i].y = 0;
    
    block_array->data[i].brick_count = 4;
    
    
    // make sure same color is never chosen twice
    do {
        block_array->data[i].color = random_color();
    }
    while(i > 0 && !strcmp(block_array->data[i-1].color, block_array->data[i].color));
    
    
    // L shape
    if(block_array->data[i].variation) {
        block_array->data[i].brick_count = 3;
        block_array->data[i].bricks[0].symbol = '@';
        block_array->data[i].bricks[1].symbol = '^';
        block_array->data[i].bricks[2].symbol = '&';
    }
    // I shape
    else {
        block_array->data[i].brick_count = 2;
        block_array->data[i].bricks[0].symbol = 'a';
        block_array->data[i].bricks[1].symbol = 'd';
        
    }

    update_bricks(&block_array->data[i]);

    // updates block array and resizes array if needed
    block_array->size++;
    if(block_array->size == block_array->capacity) {
        block_array->capacity *= 2;
        block_array->data = realloc(block_array->data, block_array->capacity * sizeof(struct tetronimo));
        if(block_array->data == NULL) {
            fprintf(stderr, "ERROR REALLOCATING BLOCK ARRAY DATA\n");
            alive = false;
        }
            
    }

}

// sets new x and y coords of each brick in a given tetronimo based on current rotation
void update_bricks(tetronimo* t) {
    if(t->rotation < 0 || t->rotation > 3) return;

    for(int i = 0; i < t->brick_count; i++) {
        if(t->variation) {
            t->bricks[i].x = t->x + L_SHAPE_ROTATION[t->rotation][i][0];
            t->bricks[i].y = t->y + L_SHAPE_ROTATION[t->rotation][i][1];
        }
        else {
            t->bricks[i].x = t->x + I_SHAPE_ROTATION[t->rotation][i][0];
            t->bricks[i].y = t->y + I_SHAPE_ROTATION[t->rotation][i][1];

        }
    }
}

// returns true if x,y is occupied not by given tetronimo
bool occupied_except(tetronimo *t, game_state *gs, int x, int y) {
    if (gs->board[y][x].symbol == ' ')
        return false;

    for (int i = 0; i < t->brick_count; i++) {
        if (t->bricks[i].x == x &&
            t->bricks[i].y == y) {
            return false;
        }
    }

    return true;
}


// called every frame on each block so bricks fall independently
void evaluate_block(tetronimo* t, game_state* gs) {
    bool shouldMove = true;
    

    for(int i = 0; i < t->brick_count; i++) {

        brick b = t->bricks[i];

        // reached bottom of board
        if(b.y + 1 >= BOARD_HEIGHT - 1) {
            shouldMove = false;
            break;
        }

        // check if position below brick is occupied by the current block
        if(occupied_except(t, gs, b.x, b.y + 1)) {
            shouldMove = false;
            break;
        }
    }

    if (shouldMove) {
        move_block(t, DOWN, gs);
        return;
    }

    if (t == current_block(gs)) {

        for (int i = 0; i < t->brick_count; i++) {
            if (t->bricks[i].y == 0) {
                alive = false;
                return;
            }
        }

        t->dead = true;
        initialize_block(gs);
    }
}

// removes single brick from a tetronimos array of bricks
void remove_brick_at(tetronimo* t, int index) {
    for(int i = index; i < t->brick_count-1; i++)
        t->bricks[i] = t->bricks[i+1];
    t->brick_count--;
}

// removes specific brick from the board
void find_and_free_brick(int row, int col, game_state* gs) {
    tetronimo_array* block_array = &gs->block_array;

    for(int i = 0; i < block_array->size; i++) {
        tetronimo* current = &block_array->data[i];
        for(int i = 0; i < current->brick_count; i++) {
            if(current->bricks[i].y == row && current->bricks[i].x == col) {
                remove_brick_at(current, i);
            }
        }

        // remove empty tetronimo
        if(current->brick_count == 0){
            memmove(current, &block_array->data[i+1], (block_array->size-(i+1)) * sizeof(struct tetronimo));
            block_array->size--;
        }
    }
}

// free every brick in a row
void clear_row(int row, game_state* gs) {
    for(int i = 0; i < BOARD_WIDTH; i++)
        find_and_free_brick(row, i, gs);
}


// updates all tetronimo blocks 
void iterate(game_state* gs) {
    tetronimo_array* block_array = &gs->block_array;

    // evaluate every tetronimo in array
    for(int i = 0; i < block_array->size; i++) {
        tetronimo* t = &block_array->data[i];
        evaluate_block(t, gs);
    }


    int cleared_rows = 0;
    // check if any rows can be cleared
    for(int i = 0; i < BOARD_HEIGHT-1; i++) {
        bool clearable = true;
        for(int j = 0; j < BOARD_WIDTH; j++) {
            if(gs->board[i][j].symbol == ' ') {
                clearable = false;
                break;
            }
        }
        if(clearable){
            cleared_rows++;
            clear_row(i, gs);
        }
    }

    gs->score += (cleared_rows * 10);
}

// returns true if block can rotate
bool can_rotate(tetronimo* t) {
    int next_rotation = (t->rotation+1) % 4;
    for(int i = 0; i < t->brick_count; i++) {
        int x, y;
        if(t->variation) {
            x = t->x +
                L_SHAPE_ROTATION[next_rotation][i][0];
            y = t->y +
                L_SHAPE_ROTATION[next_rotation][i][1];
            }
        else {
            x = t->x +
                I_SHAPE_ROTATION[next_rotation][i][0];
            y = t->y +
                I_SHAPE_ROTATION[next_rotation][i][1];

        }
        if(x <= 0 || x >= BOARD_WIDTH-1)
            return false;
        if(y >= BOARD_HEIGHT-1)
            return false;
    }   
    return true;
}


// returns true if x and y coordinate is occupied by any block other than self
bool is_occupied(tetronimo* self, int x, int y, game_state* gs) {
    tetronimo_array* block_array = &gs->block_array;
    for(int i = 0; i < block_array->size; i++) {
        tetronimo* t = &block_array->data[i];

        if(t == self)
            continue;

        for(int j = 0; j < t->brick_count; j++){
            if (t->bricks[j].x == x &&
                t->bricks[j].y == y)
                return true;
        }
    }

    return false;
}

// determines if a block can move in a specific direction
bool can_move(tetronimo* t, int dx, int dy, game_state* gs) {
    for(int i = 0; i < t->brick_count; i++) {
        int x = t->bricks[i].x + dx;
        int y = t->bricks[i].y + dy;
        if(x <= 0 || x >= BOARD_WIDTH-1)
            return false;
        if(y >= BOARD_HEIGHT-1)
            return false;
        if (is_occupied(t, x, y, gs))
            return false;
    }
    return true;
}


// move every brick in a block in a certain direction
void move_block(tetronimo* block, direction dir, game_state* gs) {
    if(dir == NONE) return;

    if(dir == ROTATE) {
        if(!can_rotate(block) || block->dead) return;
        block->rotation = (block->rotation+1) % 4;
        update_bricks(block);
        return;
    }

    int dx = 0, dy = 0;
    switch(dir) {
        case LEFT:
            dx = -1;
            break;
        case RIGHT:
            dx = 1;
            break;
        case DOWN:
            dy = 1;
            break;
        default:
            break;
    }
    if(!can_move(block, dx, dy, gs))
        return;

    if(block->dead) {
        // when bricks are dead they must be moved down without recalculating rotation
        for(int i = 0; i < block->brick_count; i++) {
            block->bricks[i].x += dx;
            block->bricks[i].y += dy;
        }
    } else {
        block->x += dx;
        block->y += dy;
        update_bricks(block);
    }
   
}


// runs in separate thread
// reads user keyboard input and moves current block
void* process_input() {
    while(alive) {
        memset(input, 0, sizeof(input));
        direction input_dir = NONE;
        
        size_t bytes_read = read(0, &input, sizeof(input));
        
        // wasd
        if(bytes_read == 1) {
            char c = input[0];
            if(c=='a'||c=='A') input_dir = LEFT;
            if(c=='d'||c=='D') input_dir = RIGHT;
            if(c=='s'||c=='S') input_dir = DOWN;
            if(c=='r'||c=='R') input_dir = ROTATE;
        }
        // arrow keys
        else if(bytes_read == 3 && input[0]=='\033' && input[1]=='[') {
            if(input[2]=='B') input_dir = DOWN;
            if(input[2]=='C') input_dir = RIGHT;
            if(input[2]=='D') input_dir = LEFT;
        }
        
        if(input_dir == NONE)
            continue;
        
        pthread_mutex_lock(&input_mutex);
            move_block(current_block(&sp_game_state), input_dir, &sp_game_state);
        pthread_mutex_unlock(&input_mutex);
    }

    pthread_exit(NULL);
    return NULL;
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char** argv) {
    srand(time(NULL));
    enable_raw_input();

    if(argc != 1 && argc != 3) {
        fprintf(stderr, "Error unrecognized argument\n");
        return 1;
    }
    if(argc == 3) {        
        // SERVER CODE
        // ./tetris -h port
        if(!strcmp(argv[1], "-h")) {
            run_server(argv[2]);
        }

            
            // CLIENT CODE
        // ./tetris -c ip:port
        if(!strcmp(argv[1], "-c")) {
            run_client(argv[2]);
        }
        else {
            perror("ERROR PARSING ARGUMENTS\n");
        }

        exit(0);
    }

    // SINGLE PLAYER MODE

    // install signal handler
    signal(SIGINT, sig_int);

    // create thread for input and mutex for it
    CHECK(pthread_attr_init(&input_t_attr));
    CHECK(pthread_create(&input_t, &input_t_attr, process_input, NULL));
    CHECK(pthread_mutex_init(&input_mutex, NULL));

    setup();

    while(alive){
        RESET_SCREEN;

        initialize_board(sp_game_state.board);
        
        CHECK(pthread_mutex_lock(&input_mutex));
            draw_blocks_to_board(&sp_game_state);
            draw_board();
            iterate(&sp_game_state);
        CHECK(pthread_mutex_unlock(&input_mutex));

        usleep(200000);
    }

    printf("GAME OVER");
    cleanup();
    return 0;
}