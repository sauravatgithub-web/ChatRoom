#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define TIMEOUT 10
#define MAX_CLIENTS 10
#define BUFFER_SIZE 256
#define EXIT_KEYWORD "EXIT"

typedef struct {
    int socket;
    char name[50];
    int report[MAX_CLIENTS];
    time_t last_active;
} Client;

Client clients[MAX_CLIENTS];

void error(const char *msg) { // error function
    perror(msg);
    exit(1);
}

void getTimeStamp(char *timestamp, size_t size) { // extracting current timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, size, "[%H:%M]", t);
}

void remove_client_from_list(int socket) { // removing a client based on socket number
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket == socket) {
            printf("Client %s (socket %d) disconnected.\n", clients[i].name, socket);
            close(socket);                                          // closing the client socket
            clients[i].socket = 0;                                  // clearing the target socket
            memset(clients[i].name, 0, sizeof(clients[i].name));    // deleting name
            break;
        }
    }
}

char* get_client_name(int socket) {                   // retrieving client based on socket number
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket == socket) return clients[i].name;
    }
    return NULL;
}

int get_client(char* name) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!strcmp(clients[i].name, name)) return i;
    }
    return -1;
}

int find_client_socket(char* name) {                  // retrieving socket number for the client
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket != 0 && strcmp(clients[i].name, name) == 0) return clients[i].socket;
    }
    return -1;
}

bool reportCheck(int ridx) {
    int total_clients = 0, count = 0;
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) total_clients++;
    }
    
    for(int j = 0; j < MAX_CLIENTS; j++) {
        if (clients[ridx].report[j]) count++;
    }

    int report;
    if(total_clients <= 2) report = 3;
    else report = (total_clients)/2 + 1;

    if(report > 5) report = 5;

    if(count >= report) return true;
    else return false;
}

void remove_idle_client(fd_set* master_fds) {
    time_t now = time(NULL);

    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket != 0 && difftime(now, clients[i].last_active) > TIMEOUT) {
            printf("Client %s Timed Out\n", clients[i].name);
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), ">> Kicked Out due to idleness...");
            send(clients[i].socket, message, strlen(message), 0);

            FD_CLR(clients[i].socket, master_fds);
            remove_client_from_list(clients[i].socket);
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // DECLARING VARIABLES - 
    int sockfd, newsockfd, portno, fdmax;       // fdmax stores the highest file descriptor
    socklen_t clilen;                           // size of client address structure cli_addr
    struct sockaddr_in serv_addr, cli_addr;
    fd_set master_fds, read_fds;                // master_fds is the set of all active sockets including server
                                                // read_fds is the set of all sockets which are ready for reading
    char buffer[BUFFER_SIZE];

    portno = atoi(argv[1]); // ASSIGNING PORT

    // STEP 1: Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    // STEP 2: Bind and listen for connections
    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    if(listen(sockfd, 5) == -1) error("ERROR on listen");

    // Setting up master file descriptor set
    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &master_fds);       // This will allow sockfd to listen from clients
    FD_SET(STDIN_FILENO, &master_fds); // Adds standard input to master_fd
    fdmax = sockfd;
    if(STDIN_FILENO > fdmax) fdmax = STDIN_FILENO;

    printf("Server listening on port %d...\n", portno);

    while(true) {
        remove_idle_client(&master_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;  // 1-second timeout
        timeout.tv_usec = 0;

        read_fds = master_fds;   // Copy master set for select
        if(select(fdmax + 1, &read_fds, NULL, NULL, &timeout) == -1) error("ERROR in select");
        // select function checks for activity across all connections and remove sockets with no activity

        // Looping through the descriptor set to check activity
        for(int i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &read_fds)) {
                
                // If a new request comes, it goes to sockfd (listening socket) and hence sockfd becomes active and is in read_fds
                if(i == sockfd) {                                    // NEW CONNECTION REQUEST
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    if(newsockfd == -1) {
                        perror("ERROR on accept");
                    } 
                    else {
                        FD_SET(newsockfd, &master_fds);              // Added in the master_fds set
                        if(newsockfd > fdmax) fdmax = newsockfd;     // Updating fdmax
                        
                        // Reading client's name
                        bzero(buffer, sizeof(buffer));
                        read(newsockfd, buffer, BUFFER_SIZE - 1);
                        buffer[strcspn(buffer, "\n")] = 0;

                        // Adding client to list
                        for(int j = 0; j < MAX_CLIENTS; j++) {
                            if(clients[j].socket == 0) {
                                clients[j].socket = newsockfd;
                                bzero(clients[j].name, sizeof(clients[j].name));
                                strncpy(clients[j].name, buffer, sizeof(clients[j].name) - 1);
                                clients[j].name[sizeof(clients[j].name) - 1] = '\0';
                                clients[j].last_active = time(NULL);

                                printf("New client connected: %s\n", clients[j].name);
                                break;
                            }
                        }
                    }
                } 
                else if(i == STDIN_FILENO) {                          // READING FROM TERMINAL
                    // message format "REMOVE username" for removing a client
                    // message format "CLOSE" for closing the server

                    memset(buffer, 0, BUFFER_SIZE);
                    fgets(buffer, BUFFER_SIZE - 1, stdin);
                    buffer[strcspn(buffer, "\n")] = 0; 
                    fflush(stdout);  

                    if(!strcasecmp(buffer, "CLOSE")) {
                        printf("~~BYE\n");
                        for(int i = 0; i <= fdmax; i++) {
                            if(FD_ISSET(i, &master_fds)) close(i);    // Closing all open sockets
                        }
                        exit(EXIT_SUCCESS);
                    }

                    if(!strncmp(buffer, "REMOVE", 6)) {
                        // extracting the target username 
                        char target_name[50];
                        bzero(target_name, sizeof(target_name));
                        sscanf(buffer, "REMOVE %s", target_name);

                        // searching the client using username and kicking the client out
                        bool found = false;
                        for(int i = 0; i < MAX_CLIENTS; i++) {
                            if(clients[i].socket != 0 && !strcmp(clients[i].name, target_name)) {
                                found = true;
                                char message[BUFFER_SIZE];
                                snprintf(message, sizeof(message), ">> Kicked Out...");
                                write(clients[i].socket, message, strnlen(message, sizeof(message))); 

                                FD_CLR(clients[i].socket, &master_fds);
                                remove_client_from_list(clients[i].socket);
                                break;
                            }
                        }
                        if(!found) {
                            printf("%s NOT FOUND IN THE CHAT...\n", target_name);
                            fflush(stdin);
                        }     
                    }
                }
                else {
                    // Reading message from a connected list
                    memset(buffer, 0, BUFFER_SIZE);
                    ssize_t n = recv(i, buffer, BUFFER_SIZE - 1, 0);     // Reading from client buffer
                    if(n <= 0) {
                        remove_client_from_list(i);
                        FD_CLR(i, &master_fds);
                        if(i == fdmax) fdmax--;  // Update fdmax if necessary
                    } 
                    else {
                        buffer[n] = '\0';
                        char *sender_name = get_client_name(i);       // Retrieving client's name
                        char message[BUFFER_SIZE + 50 + 30];
                        
                        char timestamp[30];
                        bzero(timestamp, sizeof(timestamp));
                        getTimeStamp(timestamp, sizeof(timestamp));   // Retrieving message time

                        if(!strcmp(buffer, EXIT_KEYWORD)) {           // Handling client request to exit
                            remove_client_from_list(i);
                            FD_CLR(i, &master_fds);
                            continue;
                        }

                        int index = get_client(sender_name);
                        clients[index].last_active = time(NULL);

                        // PRIVATE OR GROUP MESSAGING
                        // private message format "@username <message>"
                        // group message format "@groupname <message>"
                        if(strncmp(buffer, "@", 1) == 0) {
                            char recipient[50], msg[BUFFER_SIZE];
                            sscanf(buffer, "@%s %[^\n]", recipient, msg);
                            int recipient_socket = find_client_socket(recipient); // Retrieving recipient socket

                            if(recipient_socket != -1) {
                                bzero(message, sizeof(message));
                                snprintf(message, sizeof(message), "%s%s : %s", timestamp, sender_name, msg);
                                send(recipient_socket, message, strnlen(message, sizeof(message)), 0);  // Writing to the client
                            } 
                            else { // User not found
                                send(i, "User not found.\n", 16, 0);
                            }
                        } 
                        // REPORTING
                        else if(strncmp(buffer, "#", 1) == 0) {
                            int found = 0;
                            char reported[50];
                            sscanf(buffer, "#%s", reported);
                            int reported_socket = find_client_socket(reported); // Retrieving reported socket

                            char private_message[BUFFER_SIZE];
                            bzero(private_message, sizeof(private_message));

                            if(reported_socket != -1) {
                                found = 1;
                                if(!clients[reported_socket].report[i]) {
                                    found = 2;
                                    clients[reported_socket].report[i] = 1;
                                    if(reportCheck(reported_socket)) {         // checking if reported enough times
                                        char message[BUFFER_SIZE];
                                        snprintf(message, sizeof(message), ">> Kicked Out because of multiple reports...");
                                        send(reported_socket, message, strnlen(message, sizeof(message)), 0);    // writing to client before removal

                                        remove_client_from_list(reported_socket);            // removing reported client
                                        FD_CLR(reported_socket, &master_fds);
                                    }

                                    // Writing to client who reported
                                    snprintf(private_message, sizeof(private_message), ">> %s HAS BEEN REPORTED.", reported);
                                    if(clients[i].socket != -1) send(i, private_message, strnlen(private_message, sizeof(private_message)), 0);
                                }
                            }
                
                            if(found == 0) { // client being reported doesn't exist
                                snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", reported);
                                if(clients[i].socket != -1) send(i, private_message, strnlen(private_message, sizeof(private_message)), 0);
                            }
                            if(found == 1) { // client tried to report multiple times
                                snprintf(private_message, sizeof(private_message), ">> %s HAS ALREADY BEEN REPORTED BY YOU...", reported);
                                if(clients[i].socket != -1) send(i, private_message, strnlen(private_message, sizeof(private_message)), 0);
                            }
                        }
                        else {
                            // BROADCASTING TO ALL ALIVE USERS
                            // format "message"
                            bzero(message, sizeof(message));
                            snprintf(message, sizeof(message), "[BROADCASTING]%s%s : %s", timestamp, sender_name, buffer);

                            for(int j = 0; j <= fdmax; j++) {
                                if(FD_ISSET(j, &master_fds) && j != sockfd && j != i) {
                                    send(j, message, strnlen(message, sizeof(message)), 0);   // Writing to all alive users
                                }
                            }
                        }
                    }
                }
            }     
        }
    }

    close(sockfd);  // Closing the master socket
    return 0;
}