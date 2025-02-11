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

#define MAX_CLIENTS 10
#define BUFFER_SIZE 256

typedef struct {
    int socket;
    char name[50];
} Client;

Client clients[MAX_CLIENTS];

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void remove_client_from_list(int socket) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket == socket) {
            printf("Client %s (socket %d) disconnected.\n", clients[i].name, socket);
            clients[i].socket = 0;
            memset(clients[i].name, 0, sizeof(clients[i].name));
            close(socket);
            break;
        }
    }
}

char* get_client_name(int socket) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket == socket) return clients[i].name;
    }
}

int find_client_socket(char* name) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket != 0 && strcmp(clients[i].name, name) == 0) return clients[i].socket;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int sockfd, newsockfd, portno, fdmax;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    fd_set master_fds, read_fds;
    char buffer[BUFFER_SIZE];

    portno = atoi(argv[1]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    if(listen(sockfd, 5) == -1) error("ERROR on listen");

    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &master_fds);
    fdmax = sockfd;

    printf("Server listening on port %d...\n", portno);

    while(true) {
        read_fds = master_fds;
        if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) error("ERROR in select");

        for(int i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &read_fds)) {
                if(i == sockfd) {
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    if(newsockfd == -1) {
                        perror("ERROR on accept");
                    } 
                    else {
                        FD_SET(newsockfd, &master_fds);
                        if(newsockfd > fdmax) fdmax = newsockfd;

                        read(newsockfd, buffer, BUFFER_SIZE - 1);
                        buffer[strcspn(buffer, "\n")] = 0;

                        for(int j = 0; j < MAX_CLIENTS; j++) {
                            if(clients[j].socket == 0) {
                                clients[j].socket = newsockfd;
                                strncpy(clients[j].name, buffer, sizeof(clients[j].name) - 1);
                                printf("New client connected: %s\n", clients[j].name);
                                break;
                            }
                        }
                    }
                } 
                else {
                    memset(buffer, 0, BUFFER_SIZE);
                    ssize_t n = read(i, buffer, BUFFER_SIZE - 1);
                    printf("%s\n",buffer);
                    if(n <= 0) {
                        remove_client_from_list(i);
                        FD_CLR(i, &master_fds);
                    } 
                    else {
                        buffer[n] = '\0';
                        char *sender_name = get_client_name(i);
                        char message[BUFFER_SIZE + 50];

                        if(strncmp(buffer, "@", 1) == 0) {
                            char recipient[50], msg[BUFFER_SIZE];
                            sscanf(buffer, "@%s %[^\n]", recipient, msg);
                            int recipient_socket = find_client_socket(recipient);

                            if(recipient_socket != -1) {
                                snprintf(message, sizeof(message), "%s: %s", sender_name, msg);
                                write(recipient_socket, message, strlen(message));
                            } 
                            else {
                                write(i, "User not found.\n", 16);
                            }
                        } 
                        else {
                            snprintf(message, sizeof(message), "[BROADCAST] %s: %s", sender_name, buffer);
                            for(int j = 0; j <= fdmax; j++) {
                                if(FD_ISSET(j, &master_fds) && j != sockfd && j != i) {
                                    write(j, message, strlen(message));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(sockfd);
    return 0;
}