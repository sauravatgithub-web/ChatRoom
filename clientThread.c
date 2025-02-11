#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define BUFFER_SIZE 256

void error(char *msg){
    perror(msg);
    exit(0);
}

int sockfd;

void* listen_messages(void *arg){
    char buffer[BUFFER_SIZE];
    ssize_t n;

    while(1){
        bzero(buffer,256);
        n = read(sockfd,buffer,255);

        if(n < 0) error("ERROR reading from socket"); 

        if(n == 0) {
            printf("\nServer closed the connection. Exiting...\n");
            exit(0);
        }
        printf("\n%s\n", buffer);
        printf("Enter message: ");
        fflush(stdout);        
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){
    int portno;
    ssize_t n;

    struct sockaddr_in server_address;
    struct hostent *server;

    char buffer[BUFFER_SIZE];
    if (argc < 4) {
       fprintf(stderr, "Usage: %s <hostname> <port> <name>\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");
    
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&server_address.sin_addr.s_addr, server->h_length);
    server_address.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *)&server_address,sizeof(server_address)) < 0){
        error("ERROR connecting");
    }

    bzero(buffer,256);
    snprintf(buffer, BUFFER_SIZE, "%s", argv[3]);
    if(write(sockfd, buffer, strlen(buffer)) < 0) error("ERROR sending name");
    
    pthread_t listener_thread;
    if(pthread_create(&listener_thread, NULL, listen_messages, NULL)<0){
        error("ERROR creating thread");
    }

    while(1){
        printf("Please enter the message: ");
        bzero(buffer,256);
        fgets(buffer,255,stdin);
        n = write(sockfd,buffer,strlen(buffer));
        if (n < 0) error("ERROR writing to socket");
    }
    return 0;
}