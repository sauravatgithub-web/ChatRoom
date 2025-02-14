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

#define MAX_CLIENTS 10
#define BUFFER_SIZE 256
#define EXIT_KEYWORD "EXIT"

typedef struct {
    int socket;
    char name[50];
    int report[MAX_CLIENTS];
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
            clients[i].socket = 0;                                  // clearing the target socket
            memset(clients[i].name, 0, sizeof(clients[i].name));    // deleting name
            close(socket);                                          // closing the client socket
            break;
        }
    }
}

char* get_client_name(int socket) {                   // retrieving client based on socket number
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket == socket) return clients[i].name;
    }
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

int main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // DECLARING VARIABLES
    int sockfd, newsockfd, portno, fdmax;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    fd_set master_fds, read_fds;
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
    FD_SET(sockfd, &master_fds);
    fdmax = sockfd;

    printf("Server listening on port %d...\n", portno);

    while(true) {
        read_fds = master_fds;   // Copy master set for select
        if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) error("ERROR in select");
        // select function checks for activity across all connections

        // Looping through the descriptor set to check activity
        for(int i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &read_fds)) {

                // New connection request
                if(i == sockfd) {           
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    if(newsockfd == -1) {
                        perror("ERROR on accept");
                    } 
                    else {
                        FD_SET(newsockfd, &master_fds);
                        if(newsockfd > fdmax) fdmax = newsockfd;
                        
                        // Reading client's name
                        bzero(buffer,sizeof(buffer));
                        read(newsockfd, buffer, BUFFER_SIZE - 1);
                        buffer[strcspn(buffer, "\n")] = 0;

                        // Adding client to list
                        for(int j = 0; j < MAX_CLIENTS; j++) {
                            if(clients[j].socket == 0) {
                                clients[j].socket = newsockfd;
                                bzero(clients[j].name, sizeof(clients[j].name));
                                strncpy(clients[j].name, buffer, sizeof(clients[j].name) - 1);
                                clients[j].name[sizeof(clients[j].name) - 1] = '\0';

                                printf("New client connected: %s\n", clients[j].name);
                                break;
                            }
                        }
                    }
                } 
                else {
                    // Reading message from a connected list
                    memset(buffer, 0, BUFFER_SIZE);
                    ssize_t n = read(i, buffer, BUFFER_SIZE - 1);     // Reading from client buffer
                    if(n <= 0) {
                        remove_client_from_list(i);
                        FD_CLR(i, &master_fds);
                    } 
                    else {
                        buffer[n] = '\0';
                        char *sender_name = get_client_name(i);       // Retrieving client's name
                        char message[BUFFER_SIZE + 50 + 30];
                        
                        char timestamp[30];
                        bzero(timestamp, sizeof(timestamp));
                        getTimeStamp(timestamp, sizeof(timestamp));   // Retrieving message time

                        if(!strcmp(buffer, EXIT_KEYWORD)) { // HANDLING Client request to exit
                            remove_client_from_list(i);
                            FD_CLR(i, &master_fds);
                        }

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
                                write(recipient_socket, message, strlen(message));  // Writing to the client
                            } 
                            else { // User not in chat
                                write(i, "User not found.\n", 16);
                            }
                        } 
                        else if(strncmp(buffer, "#", 1) == 0) {
                            // REPORTING
                            int found = 0;
                            char reported[50];
                            sscanf(buffer, "#%s", reported);
                            int j = find_client_socket(reported); // Retrieving reported socket

                            char private_message[BUFFER_SIZE];
                            bzero(private_message, sizeof(private_message));

                            if(j != -1) {
                                found = 1;
                                if(!clients[j].report[i]) {
                                    found = 2;
                                    clients[j].report[i] = 1;
                                    if(reportCheck(j)) {
                                        char message[BUFFER_SIZE];
                                        snprintf(message, sizeof(message), ">> Kicked Out because of multiple reports...");
                                        write(j, message, strlen(message));    // writing to client before removed

                                        remove_client_from_list(j);            // removing reported client
                                        FD_CLR(j, &master_fds);
                                    }

                                    // Writing to client who reported
                                    snprintf(private_message, sizeof(private_message), ">> %s HAS BEEN REPORTED.", reported);
                                    if(clients[i].socket != -1) write(i, private_message, strlen(private_message));
                                }
                            }
                
                            if(found == 0) { // client being reported don't exist
                                snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", reported);
                                if(clients[i].socket != -1) write(i, private_message, strlen(private_message));
                            }
                            if(found == 1) { // client tried to report multiple times
                                snprintf(private_message, sizeof(private_message), ">> %s HAS ALREADY BEEN REPORTED BY YOU...", reported);
                                if(clients[i].socket != -1) write(i, private_message, strlen(private_message));
                            }
                        }
                        else {
                            // BROADCASTING TO ALL ALIVE USERS
                            // format "message"
                            bzero(message, sizeof(message));
                            snprintf(message, sizeof(message), "[BROADCASTING]%s%s : %s", timestamp, sender_name, buffer);

                            for(int j = 0; j <= fdmax; j++) {
                                if(FD_ISSET(j, &master_fds) && j != sockfd && j != i) {
                                    write(j, message, strlen(message));   // Writing to all alive users
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