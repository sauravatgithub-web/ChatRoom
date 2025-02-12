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

void error(char *msg){
    perror(msg);
    exit(0);
}

int sockfd;
char* fileName;

void encrypt_message(char* input, char* encrypted) {
    int len = strlen(input);
    int pos = 0;
    int last = 0;

    if(input[0] == '@') {
        while(input[last] != ' ' && input[last] != '\0') { 
            encrypted[pos++] = input[last++];
        }
        if(input[last] == ' ') encrypted[pos++] = input[last++];
    }

    if(input[last] == '@') {
        while(input[last] != ' ' && input[last] != '\0') { 
            encrypted[pos++] = input[last++];
        }
        if(input[last] == ' ') encrypted[pos++] = input[last++];
        char* fileName = (char*)malloc(sizeof(char) * (len-last+1));
        snprintf(fileName, len - last + 1, "%s", input + last);

        while(input[last] != '\0') {
            encrypted[pos++] = input[last++];
        }
        encrypted[pos++] = ' ';
        
        char* temp = realloc(input, 1024 * sizeof(char));
        input = temp;
        input[last++] = ' ';

        FILE* file = fopen(fileName, "r");
        char fileData[1024];
        fgets(fileData, sizeof(fileData), file);
        
        fclose(file);
        strcat(input, fileData);
    }
    
    len = strlen(input);
    for(int i = len - 1; i >= last; i--) {
        pos += sprintf(encrypted + pos, "%03d", (unsigned char)input[i]);
    }

    encrypted[pos] = '\0'; 
}

void decrypt_message(char* input, char* decrypted) {
    int i = 0, j = 0;
    char* fileName;
    bool gotFile = false;

    while(input[i] != ' ') decrypted[j++] = input[i++]; 
    decrypted[j++] = input[i++];
    decrypted[j++] = input[i++];
    decrypted[j++] = input[i++];

    if(input[i] == '@') {
        gotFile = true;
        i += 6;
        int k = 1;
        while(input[i + k - 1] != ' ') k++;
        strncpy(fileName, input + i, k-1);
        fileName[k-1] = '\0';
        i += k;
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

    if(!gotFile) {
        for(int k = revIndex - 1; k >= 0; k--) decrypted[j++] = reversed[k];
    }
    else {
        FILE* file = fopen(fileName, "a+");
        for(int k = revIndex - 1; k >= 0; k--) {
            fprintf(file, "%c", reversed[k]);
        }
        fclose(file);

        char message[256];
        snprintf(message, sizeof(message), "I sent you a file named %s", fileName);
        strcat(decrypted, message);
    }

    decrypted[j] = '\0';
}

void* listen_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    char decrypt[BUFFER_SIZE * 3];

    FILE* chatPad = (FILE*)arg;
    if(!chatPad) {
        perror("Error opening chat file in thread");
        return NULL;
    }

    while(true){
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = read(sockfd, buffer, BUFFER_SIZE - 1);
        if(n > 0) {
            buffer[n] = '\0';
            printf("%s\n",buffer);
            if(strcmp(buffer,"Kicked Out...")==0){
                printf("\nYou have been kicked out...\n");
                fclose(chatPad);
                _exit(EXIT_FAILURE);
            }
            decrypt_message(buffer, decrypt);
            fprintf(chatPad, "%s\n", decrypt);
            fflush(chatPad);
            fflush(stdout);
        }
        else if(n == 0) {
            printf("\nServer closed the connection. Exiting...\n");
            fclose(chatPad);
            exit(0);
        }
        else error("ERROR reading from socket"); 
            
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int portno;
    ssize_t n;
    struct sockaddr_in server_address;
    struct hostent *server;
    char buffer[BUFFER_SIZE];
    char encrypt[BUFFER_SIZE * 3];

    if(argc < 4) {
       fprintf(stderr, "Usage: %s <hostname> <port> <name>\n", argv[0]);
       exit(1);
    }
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) error("ERROR opening socket");
    
    server = gethostbyname(argv[1]);
    if(server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&server_address.sin_addr.s_addr, server->h_length);
    server_address.sin_port = htons(portno);

    if(connect(sockfd,(struct sockaddr *)&server_address,sizeof(server_address)) < 0) error("ERROR connecting");

    bzero(buffer, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE, "%s", argv[3]);

    fileName = (char*)malloc(strlen(argv[3]) + 5);
    if(!fileName) error("Memory allocation failed");
    snprintf(fileName, strlen(argv[3]) + 5, "%s.txt", argv[3]);
    printf("fileName: %s\n", fileName);

    FILE* chatPad = fopen(fileName, "a+");
    if(!chatPad) error("ERROR opening chat file");

    if(write(sockfd, buffer, strlen(buffer)) < 0) error("ERROR sending name");
    
    pthread_t listener_thread;
    if(pthread_create(&listener_thread, NULL, listen_messages, chatPad) != 0) error("ERROR creating thread");

    while(true) {
        printf("Enter message: ");
        bzero(buffer, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE ,stdin);
        buffer[strcspn(buffer, "\n")] = 0; 

        fprintf(chatPad, "Me: %s\n", buffer);
        fflush(chatPad);

        encrypt_message(buffer, encrypt);
        if(write(sockfd, encrypt, strlen(encrypt)) < 0) error("ERROR writing to socket");
    }

    fclose(chatPad);
    free(fileName);
    close(sockfd);
    return 0;
}
