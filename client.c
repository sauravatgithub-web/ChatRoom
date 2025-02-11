#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define BUFFER_SIZE 256

void error(const char* msg) {
    perror(msg);
    exit(1);
}

int socket_fd;
char* fileName;

void encrypt_message(char *input, char *encrypted) {
    int len = strlen(input);
    int pos = 0;
    int last = 0;

    if(input[0] == '@') {
        while(input[last] != ' ' && input[last] != '\0') { 
            encrypted[pos++] = input[last++];
        }
        if(input[last] == ' ') encrypted[pos++] = input[last++];
    }

    for(int i = len - 1; i >= last; i--) {
        pos += sprintf(encrypted + pos, "%03d", (unsigned char)input[i]);
    }

    encrypted[pos] = '\0'; 
}

void decrypt_message(const char *input, char *decrypted) {
    int i = 0, j = 0;

    while(input[i] != '\0' && !(input[i] >= '0' && input[i] <= '9' && input[i+1] >= '0' && input[i+1] <= '9' && input[i+2] >= '0' && input[i+2] <= '9')) {
        decrypted[j++] = input[i++];
    }

    int len = strlen(input);
    char temp[4]; 
    int revIndex = 0, revLen = (len - i) / 3;
    char reversed[revLen + 1];

    while(input[i] >= '0' && input[i] <= '9' && input[i+1] >= '0' && input[i+1] <= '9' && input[i+2] >= '0' && input[i+2] <= '9') {
        strncpy(temp, input + i, 3);
        temp[3] = '\0';
        reversed[revIndex++] = (char)atoi(temp);
        i += 3;
    }

    reversed[revIndex] = '\0';
    for(int k = revIndex - 1; k >= 0; k--) decrypted[j++] = reversed[k];

    decrypted[j] = '\0';
}

void* listen_for_messages(void* arg) {
    FILE* chatPad = (FILE*)arg;

    if(!chatPad) {
        perror("Error opening chat file in thread");
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    char decrypt[BUFFER_SIZE * 3];

    while(true) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if (n > 0) {
            buffer[n] = '\0';
            decrypt_message(buffer, decrypt);
            fprintf(chatPad, "%s\n", decrypt);
            fflush(chatPad); 
            fflush(stdout);
        } 
        else if (n == 0) {
            printf("\nServer closed the connection. Exiting...\n");
            fclose(chatPad);
            exit(0);
        } 
        else error("ERROR reading from socket");
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int port_no;
    struct sockaddr_in server_address;
    struct hostent* server;
    char buffer[BUFFER_SIZE];
    char encrypt[BUFFER_SIZE * 3];

    if(argc < 4) {
        fprintf(stderr, "Usage: %s <hostname> <port> <name>\n", argv[0]);
        exit(1);
    }

    port_no = atoi(argv[2]);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0) error("ERROR opening socket");

    server = gethostbyname(argv[1]);
    if(server == NULL) {
        fprintf(stderr, "ERROR no such host\n");
        exit(1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    memcpy(&server_address.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_address.sin_port = htons(port_no);

    if(connect(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) error("ERROR connecting");

    bzero(buffer, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE, "%s\n", argv[3]);

    fileName = (char*)malloc(strlen(argv[3]) + 5);
    if(!fileName) error("Memory allocation failed");
    snprintf(fileName, strlen(argv[3]) + 5, "%s.txt", argv[3]);
    printf("fileName: %s\n", fileName);

    FILE* chatPad = fopen(fileName, "a+");
    if(!chatPad) error("ERROR opening chat file");

    if(write(socket_fd, buffer, strlen(buffer)) < 0) error("ERROR sending name");

    pthread_t listener_thread;
    if(pthread_create(&listener_thread, NULL, listen_for_messages, chatPad) != 0) error("ERROR creating thread");

    while(true) {
        printf("Enter message: ");
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; 

        fprintf(chatPad, "Me: %s\n", buffer);
        fflush(chatPad); 

        encrypt_message(buffer, encrypt);
        if(write(socket_fd, encrypt, strlen(encrypt)) < 0) error("ERROR writing to socket");
    }

    fclose(chatPad);
    free(fileName);
    close(socket_fd);
    return 0;
}