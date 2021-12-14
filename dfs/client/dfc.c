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
#define PACKET_SIZE 1024

//hold the config files information
typedef struct {
    char username[40];
    char password[40];
    char dfs1[40];
    char dfs2[40];
    char dfs3[40];
    char dfs4[40];
} config;

typedef struct {
    int e1;
    int e2;
} tuple;

//used to check if file can be reconstructed after list call
typedef struct {
    char filename[40];
    int one;
    int two;
    int three;
    int four;
} list_check;

const tuple file_table[4][4] = {
    { {1,2}, {2,3}, {3,4}, {4,1} },
    { {4,1}, {1,2}, {2,3}, {3,4} },
    { {3,4}, {4,1}, {1,2}, {2,3} },
    { {2,3}, {3,4}, {4,1}, {1,2} }
};

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
    fclose(fp);
    return config_data;
}

/*
Computes MD5 hash of the file and then determines which servers to split the file on
*/
int md5_hash(FILE *fp) {
    int bytes, v1= 0, v2 =0, v3=0, v4=0;
    char data[1024];
    unsigned char hash[MD5_DIGEST_LENGTH];
    unsigned char digest[MD5_DIGEST_LENGTH];
    int hash_mod = 0;

    bzero(data, 1024);
    bzero(hash, MD5_DIGEST_LENGTH);
    MD5_CTX context;
    MD5_Init(&context);
    while ((bytes = fread(data, 1, 1024, fp)) > 0) {
        MD5_Update(&context, data, bytes);
    }

    bzero(hash, MD5_DIGEST_LENGTH);
    MD5_Final(hash, &context);
    bzero(digest, MD5_DIGEST_LENGTH);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&digest[i], "%02x", hash[i]);
    }

    hash_mod = strtol(digest, NULL, 0) % 4;
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
void send_command(config* config_data, int conn_fd, char* req_buffer, char* choice, char* filename) {
    ssize_t bytes_sent = -1;
    //excahnge the command and username/password
    bzero(req_buffer, 1024);
    //handle commands with filenames and ones that don't
    if (strncmp(choice, "list", 4) == 0) sprintf(req_buffer, "%s %s %s", choice, config_data->username, config_data->password);
    else sprintf(req_buffer, "%s %s %s %s", choice, filename, config_data->username, config_data->password);

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

    if (strncmp(req_buffer, "ACK", 3) == 0) {
        return;
    }

    //exit on invalid username or password
    else if (strncmp(req_buffer, "Invalid", 7) == 0) {
        printf("Invalid Username/Password. Exiting\n");
        exit(0);
    }
    
    else {
        printf("bad connection with server...\n");
        exit(-1);
    }
}

//MAKE MORE ROBUST LATER
//for now assume that each server has 2 chunks
void get(int conn_fd, char* filename) {
    FILE *new_chunk;
    char buffer[PACKET_SIZE];
    char piece[40];
    char res[5];
    int can_create[4] = {0,0,0,0};
    ssize_t bytes_read, bytes_send;
    int n;
    struct timeval timeout; 
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout);

    bzero(piece, 40);
    bytes_read = read(conn_fd, piece, 40);
    
    bzero(res, 5);
    sprintf(res, "ACK");
    write(conn_fd, res, 3);
    
    new_chunk = fopen(piece, "wb+");
    bzero(buffer, PACKET_SIZE);
    while ((n = read(conn_fd, buffer, PACKET_SIZE)) > 0) {
        fwrite(buffer, 1, n, new_chunk);
    }
    fclose(new_chunk);

    bzero(res, 5);
    sprintf(res, "ACK");
    write(conn_fd, res, 3);

    bzero(piece, 40);
    bytes_read = read(conn_fd, piece, 40);
    
    bzero(res, 5);
    sprintf(res, "ACK");
    write(conn_fd, res, 3);
    
    new_chunk = fopen(piece, "wb+");
    bzero(buffer, PACKET_SIZE);
    while ((n = read(conn_fd, buffer, PACKET_SIZE)) > 0) {
        fwrite(buffer, 1, n, new_chunk);
    }
    fclose(new_chunk);

    bzero(res, 5);
    sprintf(res, "ACK");
    write(conn_fd, res, 3);

}

//check if the file can be created and then reconstrcut it
void get_file(char* filename) {
    int can_create[4] = {0,0,0,0};
    char name[50];
    char buffer[PACKET_SIZE];
    FILE* fp, *alt_fp;
    int n = 0;

    for (int i = 0; i < 4; i++) {
        bzero(name, 50);
        sprintf(name, "%s.%d",filename, i+1);
        fp = fopen(name, "r");
        if (fp != NULL) {fclose(fp); can_create[i] = 1;}
    }

    if (can_create[0] == 0 || can_create[1] == 0 || can_create[2] == 0 || can_create[3] == 0) {
        printf("File is incomplete\n");
        return;
    }

    //save the file
    fp = fopen(filename, "wb+");
    for (int i = 0; i < 4; i++) {
        sprintf(name, "%s.%d",filename, i+1);
        alt_fp = fopen(name, "rb");
        while ( (n = fread(buffer, 1, PACKET_SIZE, alt_fp)) > 0) {
            fwrite(buffer, 1, n, fp);
        }
    }
    fclose(fp);

    printf("File added\n");
    //remove the pieces when done
    for (int i = 0; i < 4; i++) {
        bzero(name, 50);
        sprintf(name, "%s.%d",filename, i+1);
        remove(name);
    }
}

//gets the files form each server
void list_files(int conn_fd, FILE* tmp_fp) {
    char list_buffer[PACKET_SIZE];
    ssize_t bytes_read;

    bzero(list_buffer, PACKET_SIZE);
    bytes_read = read(conn_fd, list_buffer, PACKET_SIZE);
    fwrite(list_buffer, 1, bytes_read, tmp_fp);
}

//inspects the files and determines if they can be reconstructed
void print_list() {
    FILE* fp;
    list_check check[50]; //keep track of up to 50 files
    bzero(check, 50);
    char temp[50];
    int size = 0, new = 1, num;
    size_t input_size = 0;
    char* line = NULL;

    for (int i = 0; i < 50; i++) {
        check[i].one = 0;
        check[i].two = 0;
        check[i].three = 0;
        check[i].four = 0;
    }

    fp = fopen("list_temp.txt", "r");
    while ( getline(&line, &input_size, fp) > 0) {
        new = 1;
        if (size == 0) {
            bzero(check[size].filename, 40);
            bzero(temp, 50);
            //printf("%d %d %d %d\n", check[0].one, check[0].two, check[0].three, check[0].four);
            sscanf(line, "%s\n", temp);
            //printf("%s\n", line);
            num = temp[strlen(temp) -1] - '0';
            strncpy(check[size].filename, temp, strlen(temp));
            switch (num) {
                    case 1:
                        check[size].one = 1;
                        break;
                    case 2:
                        check[size].two = 1;
                        break;
                    case 3:
                        check[size].three = 1;
                        break;
                    case 4:
                        check[size].four = 1;
                        break;
            }
            size++;
        }

        else {
            bzero(temp, 50);
            sscanf(line, "%s\n", temp);
            num = temp[strlen(temp) -1] - '0';
            for (int i = 0; i < size; i++) {
                if (strncmp(temp, check[i].filename, strlen(temp) -1) == 0) {
                    //printf("%s\n", line);
                    //printf("%d %d %d %d\n", check[i].one, check[i].two, check[i].three, check[i].four);
                    switch (num) {
                        case 1:
                            check[i].one = 1;
                            //printf("In one\n");
                            break;
                        case 2:
                            check[i].two = 1;
                            //printf("IN two\n");
                            break;
                        case 3:
                            check[i].three = 1;
                            //printf("In three\n");
                            break;
                        case 4:
                            check[i].four = 1;
                            //printf("in  four\n");
                            break;
                    }
                    new=0;
                }
            }

            if (new == 1) {
                bzero(check[size].filename, 40);
                //printf("%s\n", temp);
                //printf("%d %d %d %d\n", check[size].one, check[size].two, check[size].three, check[size].four);
                strncpy(check[size].filename, temp, strlen(temp));
                switch (num) {
                    case 1:
                        check[size].one = 1;
                        break;
                    case 2:
                        check[size].two = 1;
                        break;
                    case 3:
                        check[size].three = 1;
                        break;
                    case 4:
                        check[size].four = 1;
                        break;
                }
                size++;
            }
        }
    }
    if (line) free(line);

    //printf("%d\n", check[2].p[3]);

    for (int i = 0; i < size; i++) {
        bzero(temp, 50);
        strncpy(temp, check[i].filename, strlen(check[i].filename) -2);
        //printf("%d %d %d %d\n", check[i].one, check[i].two, check[i].three, check[i].four);
        if (check[i].one == 1 && check[i].two == 1 && check[i].three == 1 && check[i].four == 1) printf("%s\n", temp);
        else printf("%s [incomplete]\n", temp);
    }

    fclose(fp);
    remove("list_temp.txt");
}


//sends the particular file to the 
void send_file(int conn_fd, int portion, long int chunk_size, int overflow, char* filename) {
    FILE *new_fp;
    ssize_t bytes_written;
    char temp_filename[40];
    char buffer[PACKET_SIZE];
    char buf[10];
    long int bytes_read = 0;
    int n;

    bzero(temp_filename, 40);
    sprintf(temp_filename, "%s.%d", filename, portion);
    new_fp = fopen(temp_filename, "rb");
    if (new_fp == NULL) {printf("File %s does not exsit\n", filename); return;} 

    //write the name of the file first
    bzero(buf, 10);
    sprintf(buf, "%d", portion);
    bytes_written = write(conn_fd, buf, 1);

    //send chunk to the server
    bzero(buffer, PACKET_SIZE);
    while ((n = fread(buffer, 1, PACKET_SIZE, new_fp)) > 0) {
        write(conn_fd, buffer, n);
    }
    fclose(new_fp);

    bzero(buf, 10);
    bytes_read = read(conn_fd,buf, 3);
    printf("%s\n", buf);
}


//chuncks the file to send to the user
void create_chunks(char* filename,int chunk_size, int overflow) {
    FILE* fp, *new_fp;
    char* buffer;
    char temp_filename[40];
    int i = 1, n;
    buffer = (char*) malloc(chunk_size);

    fp = fopen(filename, "rb");
    if (fp == NULL) {printf("File %s does not exsit\n", filename); return;} 

    bzero(buffer, chunk_size);
    while ( (n = fread(buffer, 1, chunk_size, fp)) > 0) {
        bzero(temp_filename, 40);
        sprintf(temp_filename, "%s.%d", filename, i);
        new_fp = fopen(temp_filename, "wb+");
        fwrite(buffer, sizeof(char), n, new_fp);
        if (overflow != 0) {
            n = fread(buffer, 1, overflow, fp);
            fwrite(buffer, sizeof(char), n, new_fp);
        } 
        fclose(new_fp);
        i++;
    }
    fclose(fp);
    free(buffer);
}

//deletes the temporary chunks
void free_chunks(char* filename) {
    char temp_filename[40];
    int i = 1;

    while (i < 5) {
        bzero(temp_filename, 40);
        sprintf(temp_filename, "%s.%d", filename, i);
        remove(temp_filename);
        i++;
    }
}

//put a file on the servers
void put(char* filename, int conn, int serv_num) {
    FILE* fp, *new_fp;
    int c1, c2;
    long int file_size; 
    long int chunk_size;
    int overflow = 0, hash, n;
    char* buffer;
       
    printf("sending file %s...\n", filename);
    //get the siez of the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {printf("File %s does not exsit\n", filename); return;}  
    hash = md5_hash(fp);
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);  
    fclose(fp);
        
    //break file into pieces
    overflow = file_size % 4;
    chunk_size = file_size /4;
    create_chunks(filename, chunk_size, overflow);

    c1 = file_table[hash][serv_num].e1;
    c2 = file_table[hash][serv_num].e2;
    printf("%d\n", c1);
    printf("%d\n", c2);
    send_file(conn, c1, chunk_size, overflow, filename);
    send_file(conn, c2, chunk_size, overflow, filename);
    
    free_chunks(filename);
}

//list files on a server
void handle_command(config* config_data, char* choice, char* filename) {
    int conn1, conn2, conn3, conn4;
    FILE* list_fp;
    ssize_t bytes_read;
    char req_buffer[1024];

    //store the list file names if called
    list_fp = fopen("list_temp.txt", "wb+");
    
    //connect to the four servers
    for (int i = 0; i < 4; i++) {
        switch (i) {
            case 0:
                conn1 = create_connection(config_data->dfs1);
                if (conn1 != -1)  {
                    send_command(config_data, conn1, req_buffer, choice, filename);
                    get_response(conn1, req_buffer); 
                    if (strncmp(choice, "list", 4) == 0) {
                        list_files(conn1, list_fp);
                    }

                    else if (strncmp(choice, "get", 3) == 0) {
                        get(conn1, filename);
                    }

                    else if (strncmp(choice, "put", 3) == 0) {
                        put(filename, conn1, 0);
                    }
                }
                break;
            case 1:
                conn2 = create_connection(config_data->dfs2);
                if (conn2 != -1)  {
                    send_command(config_data, conn2, req_buffer, choice, filename);
                    get_response(conn2, req_buffer); 
                    if (strncmp(choice, "list", 4) == 0) {
                        list_files(conn2, list_fp);
                    }

                    else if (strncmp(choice, "get", 3) == 0) {
                        get(conn2, filename);
                    }

                    else if (strncmp(choice, "put", 3) == 0) {
                        put(filename, conn2, 1);
                    }
                }
                break;if (strncmp(choice, "list", 4) == 0) {
                        list(conn2, list_fp);
                    }
            case 2:
                conn3 = create_connection(config_data->dfs3);
                if (conn3 != -1)  {
                    send_command(config_data, conn3, req_buffer, choice, filename);
                    get_response(conn3, req_buffer); 
                    if (strncmp(choice, "list", 4) == 0) {
                        list_files(conn3, list_fp);
                    }

                    else if (strncmp(choice, "get", 3) == 0) {
                        get(conn3,filename);
                    }

                    else if (strncmp(choice, "put", 3) == 0) {
                        put(filename, conn3, 2);
                    }
                }
                break;
            case 3:
                conn4 = create_connection(config_data->dfs4);
                if (conn4 != -1)  {
                    send_command(config_data, conn4, req_buffer, choice, filename);
                    get_response(conn4, req_buffer); 
                    if (strncmp(choice, "list", 4) == 0) {
                        list_files(conn4, list_fp);
                    }

                    else if (strncmp(choice, "get", 3) == 0) {
                        get(conn4, filename);
                    }

                    else if (strncmp(choice, "put", 3) == 0) {
                        put(filename, conn4, 3);
                    }
                }
                break;
        }
    } 
    fclose(list_fp);
    if (strncmp(choice, "list", 4) == 0) print_list();
    if (strncmp(choice, "get", 3) == 0) get_file(filename);
}

int main(int argc, char** argv) {
    config* config_data;
    char * input = NULL;
    size_t input_size = 0;
    ssize_t len;
    char filename[MAX_FILENAME_LEN], choice[5];
  
    if (argc != 2) { printf("Usage: dfc [config file (.conf)]\n"); return -1;}

    config_data = create_config(argv[1]);
    if (config_data == NULL) { printf("Error loading config file\n"); return -1;}

    //get user input
    while (1) {
        bzero(filename, MAX_FILENAME_LEN);
        prompt();
        bzero(input, input_size);
        bzero(choice, 5);

        len = getline(&input, &input_size, stdin);
        if (len == -1) {printf("Error getting input\n"); return -1;}

        if (sscanf(input, "get %s", filename) == 1) {
            strncpy(choice, "get",3);
            handle_command(config_data, choice, filename);
        } 

        else if (sscanf(input, "put %s", filename) == 1) {
            strncpy(choice, "put",3);
            handle_command(config_data, choice, filename);
        }

        else if (strncmp("list", input, 4) == 0) {
            strncpy(choice, "list",4);
            handle_command(config_data, choice, NULL);
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