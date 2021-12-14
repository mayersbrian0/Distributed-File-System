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
#include <dirent.h>
#include <openssl/md5.h>

#define PACKET_SIZE 1024

typedef struct {
    int* connection_fd;
    char* dir;
} thread_args;

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

void list() {
    printf("In list\n");
}

void get(int connection_fd) {

}

void put(int connection_fd, char* filename, char* path_to_dir) {
    ssize_t bytes_read, bytes_sent;
    char buffer[PACKET_SIZE];
    char num[2];
    char command[5];
    char new_filename[100];
    FILE* fp;

    printf("In put\n");   
    //read one byte from the buffer (the two file pieces that will be transfered)
    bzero(num, 2);
    bytes_read = read(connection_fd, num, 1);

    bzero(new_filename, 100);
    sprintf(new_filename, "%s/%s.%s",path_to_dir, filename, num);

    fp = fopen(new_filename, "wb+");
    bzero(buffer, PACKET_SIZE);
    while ((bytes_read = read(connection_fd, buffer, PACKET_SIZE)) > 0) {
        printf("%s\n", buffer);
        size_t written = fwrite(buffer,1, bytes_read, fp);
    }
    printf("%d\n", bytes_read);
    fclose(fp);

    //send ACK
    bzero(command, 5);
    sprintf(command, "ACK");
    bytes_sent = write(connection_fd, command, 3);

    
    bzero(num, 2);
    bytes_read = 0;
    bytes_read = read(connection_fd, num, 1);
    bzero(new_filename, 60);
    sprintf(new_filename, "%s/%s.%s", path_to_dir, filename, num);

    fp = fopen(new_filename, "wb+");
    bzero(buffer, PACKET_SIZE);
    while ((bytes_read = read(connection_fd, buffer, PACKET_SIZE)) > 0) {
        size_t written = fwrite(buffer,1, bytes_read, fp);
    }
    fclose(fp);

    //send ACK
    bzero(command, 5);
    sprintf(command, "ACK");
    bytes_sent = write(connection_fd, command, 3);
    
}

/*
Function to handle the clients request
*/
void handle_req(int connection_fd, char* dir) {
    FILE* fp;
    DIR *directory;
    ssize_t bytes_read, n, bytes_sent;
    char command[1024], username[40], password[40], choice[10], temp_password[40], filename[40], path_to_dir[50];
    char *line = NULL;
    size_t line_size = 0;
    int match = 0;
    struct timeval timeout; 
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(connection_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout); //set timeout when socket hangs

    bzero(command,  1024);
    bytes_read = read(connection_fd, command, 1024);
    if (bytes_read == -1) { printf("Connection error\n"); exit(-1); }

    bzero(choice, 10);
    bzero(username, 40);
    bzero(password, 40);
    bzero(filename, 40);

    if (strncmp(command, "list", 4) == 0) sscanf(command, "%s %s %s", choice, username, password);
    else sscanf(command, "%s %s %s %s", choice, filename, username, password);

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

    //send Invalid message
    if (match == 0) {
        bzero(command, 1024);
        sprintf(command, "Invalid Username/Password");
        bytes_sent = write(connection_fd, command, strlen(command));
        if (bytes_sent == -1) { printf("Connection error\n"); exit(-1); }
        return;
    }

    bzero(command, 1024);
    sprintf(command, "ACK");
    bytes_sent = write(connection_fd, command, strlen(command));
    if (bytes_sent == -1) { printf("Connection error\n"); exit(-1); }
    
    //check if the user has a directory
    bzero(path_to_dir, 50);
    sprintf(path_to_dir, "./%s/%s", dir, username);
    directory = opendir(path_to_dir);
    if (directory == NULL) {
        mkdir(path_to_dir, 0777);
    }
    closedir(directory);


    if (strncmp(choice, "get", 3) == 0) {
        get(connection_fd);
    }

    else if (strncmp(choice, "put", 3) == 0) {
        put(connection_fd, filename, path_to_dir);
        return;
    }

    else if (strncmp(choice, "list", 4) == 0)  {
        list();
    }

}

//thread routine: handle connection with individual client
void *handle_connection(void *args) {
    thread_args* vals = args;
    int connection_fd = *((int *)(vals->connection_fd));
    char* dir = vals->dir;
    pthread_detach(pthread_self()); //no need to call pthread_join()
    handle_req(connection_fd, dir);
    close(connection_fd); //client can now stop waiting
    return NULL;
}

int main(int argc, char** argv) {
    int serverfd, *connect_fd, port, clientlen=sizeof(struct sockaddr_in), timeout = -1;
    struct sockaddr_in clientaddr;
    char *ptr, *dir;
    pthread_t thread_id;

    if (argc != 3) {
        fprintf(stderr, "Usage %s [DIR] [PORT]\n", argv[0]);
        exit(0);
    }

    port = strtol(argv[2], &ptr, 10);
    if (*ptr != '\0' || port <= 1024) { printf("Invalid Port Number\n"); exit(0); } //check for errors

    dir = argv[1];
    if (dir[0] == '/') memmove(dir, dir+1, strlen(dir));

    if (strncmp(dir, "DFS1", 4) != 0 && strncmp(dir, "DFS2", 4) != 0 && strncmp(dir, "DFS3", 4) != 0 && strncmp(dir, "DFS4", 4) != 0) {printf("Invalid directory\n"); exit(0);}

    serverfd = open_serverfd(port);
    if (serverfd < 0) { printf("Error connecting to port %d\n", port); exit(0); }

    thread_args* args = malloc(sizeof(thread_args));
    args->dir = dir;
    //server terminates on ctl-c
    while (1) {
        connect_fd = malloc(sizeof(int)); //allocate space for pointer
        *connect_fd = accept(serverfd, (struct sockaddr *)&clientaddr, &clientlen); //start accepting requests
        args->connection_fd = connect_fd;
        pthread_create(&thread_id, NULL, handle_connection, args); //pass new file descripotr to thread routine
    }

    return 0;
}