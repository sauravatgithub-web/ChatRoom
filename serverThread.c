#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h> 

#define PORT 8080
#define BUFFER_SIZE 256
#define MAX_CLIENTS 10

typedef struct{
    int socket;
    char name[50];
} Client;

Client clients[MAX_CLIENTS];

void getTimeStamp(char *timestamp, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, size, "[%H:%M]", t);
}

void error(char *msg){
    perror(msg);
    exit(1);
}

void* myClientThreadFunc(void* ind){
    int index = *((int*)ind);
    free(ind);

    char buffer[BUFFER_SIZE];
    bzero(buffer, 256);

    ssize_t n = read(clients[index].socket, buffer, 255);
    if(n < 0) error("ERROR reading from socket");

    printf("%s connected to chat...\n", buffer);
    strncpy(clients[index].name, buffer, sizeof(clients[index].name) - 1);
    clients[index].name[sizeof(clients[index].name) - 1] = '\0';

    while(clients[index].socket != 0) {
        bzero(buffer, 256);
        n = read(clients[index].socket, buffer, 255);
        if(n <= 0) {
            printf("%s got disconnected from the chat...\n", clients[index].name);
            if(clients[index].socket != 0) close(clients[index].socket);
            clients[index].socket = 0;
            bzero(clients[index].name, sizeof(clients[index].name));
            pthread_exit(NULL);
        }       
        printf("%s : %s\n",clients[index].name, buffer);

        if(buffer[0] == '@') {
            // PRIVATE MESSAGE
            char target_name[50], message[BUFFER_SIZE];
            sscanf(buffer, "@%s %[^\n]", target_name, message);

            char timestamp[30];
            bzero(timestamp, sizeof(timestamp));
            getTimeStamp(timestamp, sizeof(timestamp));

            bool found = false;
            for(int i=0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != 0 && !strcmp(clients[i].name, target_name)) {
                    char private_message[BUFFER_SIZE + 50 + 30];
                    bzero(private_message, sizeof(private_message));
                    snprintf(private_message, sizeof(private_message), "%s%s : %s", timestamp, clients[index].name, message);

                    n = write(clients[i].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                    found = true;
                    break;
                }
            }
            if(!found) {
                char private_message[BUFFER_SIZE + 50 + 30];
                bzero(private_message, sizeof(private_message));
                snprintf(private_message, sizeof(private_message), "%s NOT FOUND...", target_name);

                n = write(clients[index].socket, private_message, strlen(private_message));
                if(n < 0) perror("ERROR writing to socket");
            }
        }
        else{
            //BROADCASTING
            char timestamp[30];
            bzero(timestamp, sizeof(timestamp));
            getTimeStamp(timestamp, sizeof(timestamp));

            printf("%s", timestamp);
            char message[BUFFER_SIZE + 50 + 30];
            bzero(message, sizeof(message));
            snprintf(message, sizeof(message), "[BROADCASTING]%s%s : %s", timestamp, clients[index].name, buffer);

            for(int i=0; i < MAX_CLIENTS; i++) {
                if(i != index && clients[i].socket != 0) {
                    n = write(clients[i].socket, message, strlen(message));
                    if(n < 0) perror("ERROR writing to socket");
                }
            }
        }
    }
    close(clients[index].socket);
    pthread_exit(NULL);
}

void *server_thread(void *arg){
    char buffer[BUFFER_SIZE];
    ssize_t n;

    while(true) {
        bzero(buffer, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE - 1, stdin);
        buffer[strcspn(buffer, "\n")] = 0;
        fflush(stdout);  

        char target_name[50];
        bzero(target_name, sizeof(target_name));
        sscanf(buffer, "REMOVE %s", target_name);
        printf("%s\n", target_name);  

        bool found = false;
        for (int i=0; i < MAX_CLIENTS; i++) {
            if(clients[i].socket != 0 && !strcmp(clients[i].name,target_name)) {
                char message[BUFFER_SIZE];
                snprintf(message, sizeof(message), "Kicked Out...");
                write(clients[i].socket, message, strlen(message)); 

                close(clients[i].socket);
                clients[i].socket = 0;
                found = true;
                fflush(stdin);
                break;
            }
        }
        if(!found) {
            printf("%s NOT FOUND IN THE CHAT...\n",target_name);
            fflush(stdin);
        }     
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int sockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    ssize_t n;

    if(argc < 2) portno = atoi(PORT);
    else portno = atoi(argv[1]);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) error("ERROR opening socket");
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if(bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) error("ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    printf("Server Started at port %d\n", portno);

    pthread_t serverThread;
    if(pthread_create(&serverThread, NULL, server_thread, NULL) < 0) {
        perror("Error in creating thread");
        close(sockfd);
    }
    pthread_detach(serverThread);

    while(true) {    
        int* newsockfd = malloc(sizeof(int));

        *newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if(*newsockfd < 0){
            error("ERROR on accept");
            close(*newsockfd);
            free(newsockfd);
            continue;
        }

        int client_avail = 0;
        for(int i=0; i < MAX_CLIENTS; i++) {
            if(clients[i].socket == 0){
                client_avail = 1;
                clients[i].socket = *newsockfd;

                pthread_t client_thread;
                int* client_index = malloc(sizeof(int));
                *client_index = i;

                if(pthread_create(&client_thread, NULL, myClientThreadFunc, client_index) < 0) {
                    perror("Error in creating thread");
                    close(*newsockfd);
                    free(newsockfd);
                    free(client_index);
                }
                pthread_detach(client_thread);
                break;
            }
        }
        if(client_avail == 0) {
            n = write(*newsockfd, "MAXIMUM NO. OF CLIENTS REACHED !!!", 34);
            if(n < 0) error("ERROR writing to socket");
            close(*newsockfd);
            free(newsockfd);
        }
    }

    close(sockfd);
    return 0;
}