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



int main(int argc, char** argv) {
    int serverfd, *connect_fd, port, clientlen=sizeof(struct sockaddr_in), timeout = -1;
    struct sockaddr_in clientaddr;
    char *ptr;
    pthread_t thread_id;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage %s <port> <timeout>\n", argv[0]);
        exit(0);
    }

    if (argc == 3) {
        timeout = strtol(argv[2], &ptr, 10);
        if (*ptr != '\0') { printf("Invalid Timeout\n"); exit(0); } //check for errors
    }

    port = strtol(argv[1], &ptr, 10);
    if (*ptr != '\0' || port <= 1024) { printf("Invalid Port Number\n"); exit(0); } //check for errors

    serverfd = open_serverfd(port);
    if (serverfd < 0) { printf("Error connecting to port %d\n", port); exit(0); }

    pthread_mutex_init(&ip_mutex, NULL);
    pthread_mutex_init(&page_mutex, NULL);

    struct cache_node* head = (struct cache_node*)malloc(sizeof(struct cache_node)); //pointer to the head of the BST
    memset(head->hex_string, 0, 33);

    //server terminates on ctl-c
    while (1) {
        connect_fd = malloc(sizeof(int)); //allocate space for pointer
        *connect_fd = accept(serverfd, (struct sockaddr *)&clientaddr, &clientlen); //start accepting requests
        thread_args *thread_values = malloc(sizeof(thread_values));
        thread_values->conn_fd = connect_fd;
        thread_values->tree_head = head;
        thread_values->timeout = timeout;
        pthread_create(&thread_id, NULL, handle_connection, thread_values); //pass new file descripotr to thread routine
    }

    //TODO: free with sigint handler (NOTE infinte loop)

    return 0;
}