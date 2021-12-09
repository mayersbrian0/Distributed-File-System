#include <stdio.h>
#include <stdlib.h>
#include <string.h>      
#include <strings.h>    
#include <unistd.h>      
#include <sys/socket.h>  
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <openssl/md5.h>

/*
Function to bind the server to a port
*/
int open_serverfd(int port) {

    int serverfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    //Get rid of "already in use" error
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR,  (const void *)&optval , sizeof(int)) < 0) return -1;

    //bulild serveraddr
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(serverfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) return -1;

    //reading to accept connection requests
    if (listen(serverfd, 1024) < 0) return -1;
    printf("DFS Listening on Port: %d\n", port);

    return serverfd;
}

/*
Function to handle the clients request
*/
void handle_conn(int connection_fd) {
    FILE* fp;
    ssize_t bytes_read, n, bytes_sent;
    char command[1024], username[40], password[40], choice[10], temp_password[40];
    char *line = NULL;
    size_t line_size = 0;
    int match = 0;

    bzero(command,  1024);
    bytes_read = read(connection_fd, command, 1024);
    if (bytes_read == -1) { printf("Connection error\n"); exit(-1); }

    bzero(choice, 10);
    bzero(username, 40);
    bzero(password, 40);
    sscanf(command, "%s %s %s", choice, username, password);

    fp = fopen("dfs.conf", "r");
    if (fp == NULL) { printf("Error with config file\n"); exit(-1);}

    //verify the username/password
    while (( n = getline(&line, &line_size, fp)) > 0) {
        bzero(temp_password, 40);
        sscanf(line, "%*s %s", temp_password);
        if (strncmp(line, username, strlen(username)) == 0 && strncmp(temp_password, password, strlen(password)) == 0) {
            match = 1;
            break;
        }
    }
    if (line) free(line);
    fclose(fp);

    if (match == 0) {
        bzero(command, 1024);
        sprintf(command, "Invalid Username/Password");
        bytes_sent = write(connection_fd, command, strlen(command));
        if (bytes_sent == -1) { printf("Connection error\n"); exit(-1); }
        return;
    }

    printf("HOWS BAPA\n");
}

//thread routine: handle connection with individual client
void *handle_connection(void *thread_args) {
    int connection_fd = *((int *)thread_args);
    pthread_detach(pthread_self()); //no need to call pthread_join()
    free(thread_args); //free space
    handle_conn(connection_fd);
    close(connection_fd); //client can now stop waiting
    return NULL;
}

int main(int argc, char** argv) {
    int serverfd, *connect_fd, port, clientlen=sizeof(struct sockaddr_in), timeout = -1;
    struct sockaddr_in clientaddr;
    char *ptr;
    pthread_t thread_id;

    if (argc != 3) {
        fprintf(stderr, "Usage %s [DIR] [PORT]\n", argv[0]);
        exit(0);
    }

    port = strtol(argv[2], &ptr, 10);
    if (*ptr != '\0' || port <= 1024) { printf("Invalid Port Number\n"); exit(0); } //check for errors

    serverfd = open_serverfd(port);
    if (serverfd < 0) { printf("Error connecting to port %d\n", port); exit(0); }

    //server terminates on ctl-c
    while (1) {
        connect_fd = malloc(sizeof(int)); //allocate space for pointer
        *connect_fd = accept(serverfd, (struct sockaddr *)&clientaddr, &clientlen); //start accepting requests
        pthread_create(&thread_id, NULL, handle_connection, connect_fd); //pass new file descripotr to thread routine
    }

    return 0;
}