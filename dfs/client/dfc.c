#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <openssl/md5.h>

#define MAX_FILENAME_LEN 50

//hold the config files information
typedef struct {
    char username[40];
    char password[40];
    char dfs1[40];
    char dfs2[40];
    char dfs3[40];
    char dfs4[40];
} config;

//prompt for user
void prompt() {
    printf("\nCommand Options: \n------------ \n list \n get [file_name] \n put [file_name]\n exit\n------------\n>> ");
}

/*
read the config file and gets all relevant data
Because of time constraints, I will just assume that dfc.conf has a valid format
*/
config* create_config(char* cfg_file) {
    FILE* fp;
    char* line = NULL;
    size_t line_size = 0;
    ssize_t bytes_read;
    int count = 0;
    config* config_data = (config *)malloc(sizeof(config));

    //zero the buffers
    bzero(config_data->dfs1, 40);
    bzero(config_data->dfs2, 40);
    bzero(config_data->dfs3, 40);
    bzero(config_data->dfs4, 40);
    bzero(config_data->username, 40);
    bzero(config_data->password, 40);

    fp = fopen(cfg_file, "r");
    if (fp == NULL) return NULL;

    while ((bytes_read = getline(&line, &line_size, fp)) > 0) {
        switch (count) {
            case 0:
                sscanf(line, "%s", config_data->dfs1);
                break;
            case 1:
                sscanf(line, "%s", config_data->dfs2);
                break;
            case 2: 
                sscanf(line, "%s", config_data->dfs3);
                break;
            case 3:
                sscanf(line, "%s", config_data->dfs4);
                break;
            case 4:
                sscanf(line, "%*s %s", config_data->username);
                break;
            case 5:
                sscanf(line, "%*s %s", config_data->password);
                break;
            default:
                break;
        }
        count++;        
    }
    if (line) free(line);

    /*
    printf("%s\n", config_data->dfs1);
    printf("%s\n", config_data->dfs2);
    printf("%s\n", config_data->dfs3);
    printf("%s\n", config_data->dfs4);
    printf("%s\n", config_data->username);
    printf("%s\n", config_data->password);
    */

    fclose(fp);
    return config_data;
}

/*
Computes MD5 hash of the file and then determines which servers to split the file on
*/
int md5_hash(FILE* fp) {
    int bytes, v1, v2, v3, v4, nhash;
    char data[1024];
    char hash[MD5_DIGEST_LENGTH];
    int hash_mod = 0;

    bzero(data, 1024);
    MD5_CTX context;
    MD5_Init(&context);
    while ((bytes = fread(data, 1, 1024, fp)) > 0) {
        MD5_Update(&context, data, bytes);
        bzero(data, 1024);
    }
    MD5_Final(hash, &context);

    sscanf( &hash[0], "%x", &v1 );
    sscanf( &hash[8], "%x", &v2 );
    sscanf( &hash[16], "%x", &v3 );
    sscanf( &hash[24], "%x", &v4 );
    hash_mod = (v1 ^ v2 ^ v3 ^ v4) % 4;

    printf("%d\n", hash_mod);
    return hash_mod;
}

/*
Connects to servers listed in dfs.conf
*/
int create_connection(char* ip_port) {
    int sockfd, connfd, port_num;
    struct sockaddr_in servaddr, cli;
    struct hostent *server;
    char temp[40];
    bzero(temp, 40);
    strncpy(temp,ip_port, strlen(ip_port));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) { printf("Error Creating Socket\n"); exit(-1);}
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    for (char* p = temp; *p; p++) {
        if (*p == ':') {
            port_num = atoi(p +1); //get the port number 
            *p = '\0'; //null out port info
            break;
        }
    }

    strtok(temp, ":");
    server = gethostbyname(temp); //get the ip addr
    bcopy((char *)server->h_addr_list[0], (char *)&servaddr.sin_addr.s_addr, server->h_length);
    servaddr.sin_port = htons(port_num);
    if (connect(sockfd, (struct sockaddr_in*)&servaddr, sizeof(servaddr)) != 0) { printf("Connection failed with %s:%d\n", temp, port_num); return -1;} 
    
    return sockfd;
}

//send command to server(s)
void send_command(config* config_data, int conn_fd, char* req_buffer) {
    ssize_t bytes_sent = -1;
    //excahnge the command and username/password
    bzero(req_buffer, 1024);
    sprintf(req_buffer, "list %s %s", config_data->username, config_data->password);
    bytes_sent = write(conn_fd, req_buffer, strlen(req_buffer));
    if (bytes_sent == -1) { printf("Connection error\n"); exit(-1); }
}

//get response from server(s)
void get_response(int conn_fd, char* req_buffer) {
    ssize_t bytes_read;
    //wait for response
    bzero(req_buffer, 1024);
    bytes_read = read(conn_fd, req_buffer, 1024);
    if (bytes_read == -1) { printf("Connection error\n"); exit(-1); }
    printf("%s\n", req_buffer);
}

//list files on a server
void list(config* config_data) {
    int conn1, conn2, conn3, conn4;
    ssize_t bytes_read;
    char req_buffer[1024];

    //connect to the four servers
    for (int i = 0; i < 4; i++) {
        switch (i) {
            case 0:
                conn1 = create_connection(config_data->dfs1);
                break;
            case 1:
                conn2 = create_connection(config_data->dfs2);
                break;
            case 2:
                conn3 = create_connection(config_data->dfs3);
                break;
            case 3:
                conn4 = create_connection(config_data->dfs4);
                break;
        }
    }
    
    //send the command and username/password to open servers
    for (int i = 0; i < 4; i++) {
        switch (i) {
            case 0:
                if (conn1 != -1) send_command(config_data, conn1, req_buffer);
                break;
            case 1:
                if (conn2 != -1) send_command(config_data, conn2, req_buffer);
                break;
            case 2:
                if (conn3 != -1) send_command(config_data, conn3, req_buffer);
                break;
            case 3:
                if (conn3 != -1) send_command(config_data, conn4, req_buffer);
                break;
        }
    }   
    
    //get response from servers: either invalid or confirmation about sending the file
    for (int i = 0; i < 4; i++) {
        switch (i) {
            case 0:
                if (conn1 != -1) get_response(conn1, req_buffer);
                break;
            case 1:
                if (conn2 != -1) get_response(conn2, req_buffer);
                break;
            case 2:
                if (conn3 != -1) get_response(conn3, req_buffer);
                break;
            case 3:
                if (conn3 != -1) get_response(conn4, req_buffer);
                break;
        }
    }

    //close(conn1);
    //close(conn2);
    //close(conn3);
    //close(conn4);
    
}

//put a file on the servers
void put() {

}

//get a file from the server
void get() {

}

//attemps to verify credentials with the servers
int send_credentials() {
    return 0;
}

int main(int argc, char** argv) {
    config* config_data;
    char * input = NULL;
    size_t input_size = 0;
    ssize_t len;
    char filename[MAX_FILENAME_LEN];

    if (argc != 2) { printf("Usage: dfc [config file (.conf)]\n"); return -1;}

    config_data = create_config(argv[1]);
    if (config_data == NULL) { printf("Error loading config file\n"); return -1;}

    //get user input
    while (1) {
        bzero(filename, MAX_FILENAME_LEN);
        prompt();
        bzero(input, input_size);
        len = getline(&input, &input_size, stdin);
        if (len == -1) {printf("Error getting input\n"); return -1;}

        if (sscanf(input, "get %s", filename) == 1) {
            printf("get\n");
            printf("%s\n", filename);
            get();
        } 

        else if (sscanf(input, "put %s", filename) == 1) {
            printf("put\n");
            printf("%s\n", filename);
            put();
        }

        else if (strncmp("list", input, 4) == 0) {
            list(config_data);
        }

        else if (strncmp("exit", input, 4) == 0) {
            break;
        }

        else {
            printf("Invalid Command\n");
        }
    }
    
    if (input) free(input); //free the line

    return 0;
}