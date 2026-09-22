#include "tetris.h"

void run_server(char* portNo) {

    // get port no. from arguments
    int port = (int)strtol(portNo, NULL, 10);

    // create socket
    int server_fd;
    CHECK(server_fd = socket(PF_INET, SOCK_STREAM, 0));
    
    // enables local address reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
    
    // configure server address structure
    struct sockaddr_in address;
    int addrres_len = sizeof(address);
    address.sin_family = PF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    CHECK(bind(server_fd, (struct sockaddr*) &address, addrres_len));
    CHECK(listen(server_fd, 10)) // accept up to 10 connection

    set_nonblocking(server_fd);

    // loop and wait until host presses play (c - continue)
    bool game_started = false;
    int games_finished = 0;

    struct pollfd fds[MAX_CLIENTS] = {0};
    for(int i = 0; i < MAX_CLIENTS; i++) // initialize fds array 
        fds->fd = -1;

    int num_fds = 0;
    
    printf("STARTED TETRIS SERVER ON PORT %d\n", port);
    printf("waiting for connections...\n\n");

    while(!game_started) {
        int client_fd;
        // accept new client connection
        if((client_fd  = accept(server_fd, (struct sockaddr*) &address, (socklen_t*)&addrres_len)) >= 0) {
            set_nonblocking(client_fd);

            if(num_fds < MAX_CLIENTS+1) {
                fds[num_fds].fd = client_fd;
                fds[num_fds].events = POLL_IN;
                printf("New player connected (%d)...\n", client_fd);
                num_fds++;
            }
            else {
                printf("Server full. Rejecting connection.\n");
                close(client_fd);
            }

            if(num_fds > 0) 
                printf("Press 'C' to continue\n");
            
            
        }
        
        // check if host wants to start game
        if(num_fds > 0) {
            char c = {0};
            int bytes_read = read(0, &c, 1);
            if(bytes_read > 0 && (c == 'c' || c == 'C')) {
                game_started = true;
            }
        }
    }

    // create num_fds game states
    game_state* all_games = calloc(num_fds, sizeof(game_state));
    for(int i = 0; i < num_fds; i++) {
        all_games[i].block_array.capacity = 256;
        all_games[i].block_array.size = 0;
        all_games[i].block_array.data = calloc(256, sizeof(tetronimo));
        all_games[i].score = 0;
        all_games[i].alive = true;
        initialize_board(all_games[i].board);
        all_games[i].dir = NONE;
        initialize_block(&all_games[i]);
        
    }

    // create server_board to send to gamers
    pixel** server_board = calloc(BOARD_HEIGHT+1, sizeof(pixel*));
    int server_board_width = (BOARD_WIDTH * num_fds); 

    for(int i = 0; i < BOARD_HEIGHT+1; i++) {
        server_board[i] = calloc(server_board_width, sizeof(pixel));

        if(i == 0) {
            for(int j = 0; j < server_board_width; j++) {
                if((j % BOARD_WIDTH) < 5){
                    char* score = "SCORE";
                    server_board[i][j].symbol = score[j % BOARD_WIDTH];
                    strncpy(server_board[i][j].color, HWHT, 7);
                    
                }
                else {
                    server_board[i][j].symbol = ' ';
                    strncpy(server_board[i][j].color, HWHT, 7);
                }
            }

        }
        else
            for(int j = 0; j < server_board_width; j++) {
                server_board[i][j].symbol = ' ';
                strncpy(server_board[i][j].color, HWHT, 7);
            }
    }
    
    printf("created %d game_states\n", num_fds);
    // send server size to gamers
    for(int i = 0; i < num_fds; i++) {
        send(fds[i].fd, &server_board_width, sizeof(int), 0);
    }


    // start game
    // loop
        // process input from clients
        // update game states
        // assemble board
        // send new board to clients
    while(game_started) {
        int ready_fds = poll(fds, num_fds, 10);

        // reset direction
        for(int i = 0; i < num_fds; i++) all_games[i].dir = NONE;

        // process input from gamers
        if(ready_fds > 0){
            for(int i = 0; i < num_fds; i++) {
                // received directionn data
                if(fds[i].revents && fds[i].revents == POLLIN) {
                    memset(input, 0, sizeof(input));
                    direction input_dir = NONE;
                    
                    size_t bytes_read = recv(fds[i].fd, &input, sizeof(input), 0);
                    if(bytes_read) {

                        char c = input[0];
                        // wasd
                        if(c=='a'||c=='A') input_dir = LEFT;
                        if(c=='d'||c=='D') input_dir = RIGHT;
                        if(c=='s'||c=='S') input_dir = DOWN;
                        if(c=='r'||c=='R') input_dir = ROTATE;
                        // arrow keys
                        if(c=='\033' && input[1]=='[') {
                            if(input[2]=='B') input_dir = DOWN;
                            if(input[2]=='C') input_dir = RIGHT;
                            if(input[2]=='D') input_dir = LEFT;
                        }
                    }

                    all_games[i].dir = input_dir;
                    // printf("player %d is moving %d\n", i, input_dir);

                }
            }
        }   
            
        // update game states
        for(int i = 0; i < num_fds; i++) { 
            if(all_games[i].alive == false) continue;

            iterate(&all_games[i]);
            move_block(current_block(&all_games[i]), all_games[i].dir, &all_games[i]);
        }
            
        // assemble board
        for(int i = 0; i < num_fds; i++) {
            initialize_board(all_games[i].board);
            draw_blocks_to_board(&all_games[i]);

            // draw score to board
            int score = all_games[i].score;
            int c = 1;
            while(score >= 0) {
                int end = score % 10;
                server_board[0][(i+1) * BOARD_WIDTH - c -1].symbol = end + '0';
                c++;
                score = score / 10;
                if (score == 0) score--;
            }

            // draw rest of board
            for(int r = 1; r < BOARD_HEIGHT+1; r++) {
                for(int c = 0; c < BOARD_WIDTH; c++) {
                    server_board[r][c + (i * (BOARD_WIDTH))] = all_games[i].board[r][c];
                }
            }
        }

        // send board to clients
        for(int i = 0; i < num_fds; i++) {
            // because its not in a contiguous block of data we have to send each line at a time
            for(int j = 0; j < BOARD_HEIGHT+1; j++) {
                send(fds[i].fd, server_board[j], sizeof(pixel) * (BOARD_WIDTH * num_fds), 0);
            }
        }

        // check if game is over
        for(int i = 0; i < num_fds; i++) {
            if(all_games[i].alive == false) continue;

            tetronimo* curr_block = current_block(&all_games[i]);
            for(int j = 0; j < all_games[i].block_array.size; j++) {

                tetronimo block = all_games[i].block_array.data[j];
                if(curr_block != &block)
                    for(int k = 0; k < block.brick_count; k++) {
                        if(block.bricks[k].y == 0){
                            all_games[i].alive = false;
                            games_finished++;
                        }
                    }
            }
        }

        if(games_finished == num_fds) {
            game_started = false;
        }
        
        usleep(200000);                
    }

    exit(0);

}
