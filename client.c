#include "tetris.h"

void run_client(char* ipNo) {

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    char* ip = strtok(ipNo, ":");
    int port = atoi(strtok(NULL, ":"));
    printf("Connecting to %s:%d...\n", ip, port);

    
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &server_address.sin_addr) <= 0) {
        perror("INVALID ADDRESS");
        close(client_fd);
        exit(1);
    }
    
    // connect to server
    int connection_status = connect(client_fd, (struct sockaddr*)&server_address, sizeof(server_address));
    if(connection_status < 0) {
        perror("ERROR CONNECTING CLIENT TO SERVER\n");
        close(client_fd);
        exit(1);
    }
    
    printf("CONNECTED TO SERVER\n");
    int board_width;
    read(client_fd, &board_width, sizeof(int));
    
    
    pixel** board = calloc(BOARD_HEIGHT+1, sizeof(pixel));
    for(int i = 0; i < BOARD_WIDTH+1; i++) {
        board[i] = calloc(board_width, sizeof(pixel));
    }
    
    pixel* pixel_row = calloc(board_width, sizeof(pixel));
    bool game_started = true;
    while(game_started) {
        // process client input
        memset(input, 0, sizeof(input));
        
        // read from player and send to the server
        size_t bytes_read = read(0, &input, sizeof(input));

        if(bytes_read){
            send(client_fd, input, sizeof(input), 0);
            // printf("sending %s\n", input);
        } 

        // read each row at a time
        RESET_SCREEN;
        for(int i = 0; i < BOARD_HEIGHT+1; i++) {
            int bytes_read = recv(client_fd, pixel_row, board_width * sizeof(pixel), 0);
            if(bytes_read == board_width * sizeof(pixel)){
                for(int j = 0; j < board_width; j++) {
                    printf("%s%c", pixel_row[j].color, pixel_row[j].symbol);
                }
                printf("\n");
            }
            else 
                game_started = false;
        }
    }
}