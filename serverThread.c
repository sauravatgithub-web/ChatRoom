#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE 256
#define MAX_CLIENTS 10

typedef struct{
    int socket;
    char name[50];
}Client;

Client clients[MAX_CLIENTS];

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void *myClientThread(void* ind){
    int index = *((int*)ind);
    free(ind);

    char buffer[BUFFER_SIZE];
    ssize_t n;

    bzero(buffer,256);
    n = read(clients[index].socket,buffer,255);
    if (n < 0) error("ERROR reading from socket");
    printf("%s connected to chat...\n",buffer);
    strncpy(clients[index].name, buffer, sizeof(clients[index].name) - 1);
    clients[index].name[sizeof(clients[index].name) - 1] = '\0';

    while(1){
        bzero(buffer,256);
        n = read(clients[index].socket,buffer,255);
        if (n <= 0) {
            printf("%s got disconnected from the chat...\n",clients[index].name);
            close(clients[index].socket);
            clients[index].socket = 0;
            bzero(clients[index].name, sizeof(clients[index].name));
            pthread_exit(NULL);
        }       
        printf("%s : %s\n",clients[index].name,buffer);

        if(buffer[0] == '@'){
            // PRIVATE MESSAGE
            char target_name[50],message[BUFFER_SIZE];
            sscanf(buffer, "@%s %[^\n]", target_name, message);

            for (int i=0;i<MAX_CLIENTS; i++){
                if(clients[i].socket != 0 && strcmp(clients[i].name,target_name)==0){
                    char private_message[BUFFER_SIZE+50];
                    bzero(private_message,sizeof(private_message));
                    snprintf(private_message,sizeof(private_message),"%s : %s",clients[index].name,message);

                    n = write(clients[i].socket,private_message,strlen(private_message));
                    if (n < 0) perror("ERROR writing to socket");
                }
            }
        }
        else{
            //BROADCASTING
            char message[BUFFER_SIZE+50];
            bzero(message,sizeof(message));
            snprintf(message,sizeof(message),"[BROADCASTING] %s : %s",clients[index].name,buffer);

            for(int i=0;i<MAX_CLIENTS;i++){
                if(i!=index && clients[i].socket!=0){
                    n = write(clients[i].socket,message,strlen(message));
                    if (n < 0) perror("ERROR writing to socket");
                }
            }
        }
    }
    close(clients[index].socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int sockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    ssize_t n;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    while(1){    
        int *newsockfd = malloc(sizeof(int));

        *newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (*newsockfd < 0){
            error("ERROR on accept");
            close(*newsockfd);
            free(newsockfd);
            continue;
        }

        int client_avail = 0;
        for(int i=0; i<MAX_CLIENTS; i++){
            if(clients[i].socket == 0){
                client_avail=1;
                clients[i].socket = *newsockfd;

                pthread_t client_thread;
                int *client_index = malloc(sizeof(int));
                *client_index = i;

                if(pthread_create(&client_thread,NULL,myClientThread,client_index)<0){
                    perror("Error in creating thread");
                    close(*newsockfd);
                    free(newsockfd);
                    free(client_index);
                }
                pthread_detach(client_thread);
                break;
            }
        }
        if(client_avail==0){
            n = write(*newsockfd,"MAXIMUM NO. OF CLIENTS REACHED !!!",34);
            if(n < 0) error("ERROR writing to socket");
            close(*newsockfd);
            free(newsockfd);
        }
    }
    close(sockfd);
    return 0;
}