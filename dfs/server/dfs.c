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

//thread routine: handle connection with individual client
void *handle_connection(void *thread_args) {
    int connection_fd = *((int *)thread_args);
    pthread_detach(pthread_self()); //no need to call pthread_join()
    free(thread_args); //free space
    printf("Here\n");
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

    //TODO: free with sigint handler (NOTE infinte loop)

    return 0;
}