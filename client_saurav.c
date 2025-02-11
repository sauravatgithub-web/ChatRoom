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

void encrypt_message(char *input, char *encrypted) {
    int len = strlen(input);
    int pos = 0;
    int last = 0;

    // If input starts with '@', copy characters until the first space
    if (input[0] == '@') {
        while (input[last] != ' ' && input[last] != '\0') { // Ensure not to go out of bounds
            encrypted[pos++] = input[last++];
        }
        if (input[last] == ' ') { // Copy the space too
            encrypted[pos++] = input[last++];
        }
    }

    // Reverse and convert remaining characters to three-digit ASCII codes
    for (int i = len - 1; i >= last; i--) {
        pos += sprintf(encrypted + pos, "%03d", (unsigned char)input[i]);
    }

    encrypted[pos] = '\0'; // Null-terminate the string
}

void decrypt_message(const char *input, char *decrypted) {
    int i = 0, j = 0;

    // Find where the encryption starts (first occurrence of a three-digit ASCII code)
    while (input[i]!='\0' && !(input[i]>='0' && input[i]<='9' && input[i+1] >= '0' && input[i+1]<='9' && input[i + 2]>='0' && input[i+2]<='9')){
        decrypted[j++] = input[i++]; // Copy non-encrypted part
    }

    // Process encrypted portion
    int len = strlen(input);
    char temp[4]; // Buffer for 3-digit ASCII codes
    int revIndex = 0, revLen = (len - i) / 3;
    char reversed[revLen + 1];

    // Convert ASCII codes back to characters and store in reversed order
    while (input[i] >= '0' && input[i] <= '9' && input[i + 1] >= '0' && input[i + 1] <= '9' && input[i + 2] >= '0' && input[i + 2] <= '9') {
        strncpy(temp, input + i, 3);
        temp[3] = '\0';
        reversed[revIndex++] = (char)atoi(temp);
        i += 3;
    }

    reversed[revIndex] = '\0';

    // Reverse back to original order
    for (int k = revIndex - 1; k >= 0; k--) {
        decrypted[j++] = reversed[k];
    }


    decrypted[j] = '\0';
}

void* listen_for_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    char decrypt[BUFFER_SIZE*3];
    while(true) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if(n > 0) {
            buffer[n] = '\0';  
            decrypt_message(buffer, decrypt);
            printf("\n%s\n", decrypt);
            printf("Enter message: ");
            fflush(stdout);
        } 
        else if(n == 0) {
            printf("\nServer closed the connection. Exiting...\n");
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
    char encrypt[BUFFER_SIZE*3];

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

    snprintf(buffer, BUFFER_SIZE, "%s\n", argv[3]);
    if(write(socket_fd, buffer, strlen(buffer)) < 0) error("ERROR sending name");

    pthread_t listener_thread;
    if(pthread_create(&listener_thread, NULL, listen_for_messages, NULL) != 0) error("ERROR creating thread");

    while (1) {
        printf("Enter message: ");
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);

        buffer[strcspn(buffer, "\n")] = 0; 
        encrypt_message(buffer, encrypt);
        if(write(socket_fd, encrypt, strlen(encrypt)) < 0) error("ERROR writing to socket");
    }

    close(socket_fd);
    return 0;
}