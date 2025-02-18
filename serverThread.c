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

#define BUFFER_SIZE 256
#define MAX_CLIENTS 10
#define MAX_GROUPS 10
#define TIMEOUT 10000
#define EXIT_KEYWORD "EXIT"

typedef struct{
    int socket;
    char name[50];
    int report[MAX_CLIENTS];
    time_t last_active;
} Client;

typedef struct group{
    int groupID;
    char groupName[50];
    int indexNumbers[MAX_CLIENTS];
} Group;

Client clients[MAX_CLIENTS];
Group groups[MAX_GROUPS];

// function to get the current time 
void getTimeStamp(char *timestamp, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, size, "[%H:%M]", t);
}

void *timeout_checker(void *arg) {
    while (1) {
        sleep(1);
        time_t now = time(NULL);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != 0 && difftime(now, clients[i].last_active) > TIMEOUT) {
                printf("Client %s Timed Out\n", clients[i].name);
                char message[BUFFER_SIZE];
                int nz = snprintf(message, sizeof(message), ">> Kicked Out due to idleness...");
                write(clients[i].socket, message, strlen(message));

                if(clients[i].socket!=0) close(clients[i].socket);
                clients[i].socket = 0;
            }
        }
        
        //NEW UTILS HERE
        //Clearing the groups not in use...
        for(int i=0;i<MAX_GROUPS;i++){
            if(groups[i].groupID!=0){
                int available=0;
                for(int j=0;j<MAX_CLIENTS;j++){
                    if(groups[i].indexNumbers[j]!=-1){
                        available=1;
                        break;
                    }
                }
                if(available==0){
                    bzero(groups[i].groupName, sizeof(groups[i].groupName));
                    groups[i].groupID=0;
                }
            }
        }
    }
    pthread_exit(NULL);
}

void REMOVE_CLIENT_FROM_EVERY_GROUP(int client_index){
    for(int i=0;i<MAX_GROUPS;i++){
        for(int j=0;j<MAX_CLIENTS;j++){
            if(groups[i].indexNumbers[j]==client_index) groups[i].indexNumbers[j]=-1;
        }
    }
}

void error(char *msg){
    perror(msg);
    exit(1);
}

void reportCheck(int ridx) {
    int total_clients = 0, count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) total_clients++;
    }
    
    for (int j = 0; j < MAX_CLIENTS; j++) {
        if (clients[ridx].report[j]) count++;
    }

    int report;
    if (total_clients <= 2) report = 3;
    else report = (total_clients)/2 + 1;

    if (report > 10) report = 10;

    if (count >= report) {
        printf("%s has been removed due to reports.\n", clients[ridx].name);

        char message[BUFFER_SIZE];
        int nz = snprintf(message, sizeof(message), ">> Kicked Out because of multiple reports...");
        write(clients[ridx].socket, message, strlen(message)); 

        close(clients[ridx].socket);
        clients[ridx].socket = 0;
    }
}

void* myClientThreadFunc(void* ind){
    int index = *((int*)ind);
    free(ind);

    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    // reading the request for connection from client
    ssize_t n = read(clients[index].socket, buffer, BUFFER_SIZE - 1);
    if(n < 0) error("ERROR reading from socket");

    bool found = false;
    // looping through all client and matching the name
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket != 0 && !strcmp(clients[i].name, buffer)) {
            found = true;
            break;
        }
    }
    if(found) {
        // if target client is not found
        char private_message[BUFFER_SIZE + 50 + 30];
        bzero(private_message, sizeof(private_message));
        int nz = snprintf(private_message, sizeof(private_message), ">> USERNAME HAS ALREADY BEEN TAKEN...");

        n = write(clients[index].socket, private_message, strlen(private_message));
        if(n < 0) perror("ERROR writing to socket");

        close(clients[index].socket);
        clients[index].socket = 0;
        bzero(clients[index].name, sizeof(clients[index].name));
        pthread_exit(NULL);
    }

    printf("%s connected to chat...\n", buffer);

    // fetching the name of connected client 
    strncpy(clients[index].name, buffer, sizeof(clients[index].name) - 1);
    clients[index].name[sizeof(clients[index].name) - 1] = '\0';
    
    // Report array initialized for this client
    bzero(clients[index].report, sizeof(clients[index].report));

    clients[index].last_active = time(NULL);

    // reading the request from client while it is connected
    while(clients[index].socket != 0) {
        bzero(buffer, 256);

        // reading the message received from client
        n = read(clients[index].socket, buffer, 255);
        if(n <= 0 || !strcmp(buffer, EXIT_KEYWORD)) {
            // client disconnected
            // freeing the socket and exiting the pthread
            printf("%s got disconnected from the chat...\n", clients[index].name);
            REMOVE_CLIENT_FROM_EVERY_GROUP(index);      //removing the client from every group when disconnected...
            if(clients[index].socket != 0) close(clients[index].socket);
            clients[index].socket = 0;
            bzero(clients[index].name, sizeof(clients[index].name));
            pthread_exit(NULL);
        }       
        // printf("%s : %s\n",clients[index].name, buffer);

        clients[index].last_active = time(NULL);

        if(buffer[0] == '@') {
            // --PRIVATE MESSAGE--
            // message format "@username <message>"

            // extracting the target name and message from buffer
            char target_name[50], message[BUFFER_SIZE];
            sscanf(buffer, "@%s %[^\n]", target_name, message);
            
            if(strcmp(target_name,"SHOW_ALL")==0){
                // SHOWALL command to show all the clients connected to the server
                char private_message[BUFFER_SIZE];
                bzero(private_message, sizeof(private_message));
                int nz = snprintf(private_message, sizeof(private_message), ">> CLIENTS CONNECTED TO THE SERVER...");

                n = write(clients[index].socket, private_message, strlen(private_message));
                if(n < 0) perror("ERROR writing to socket");

                for(int i=0; i < MAX_CLIENTS; i++) {
                    if(clients[i].socket != 0) {
                        bzero(private_message, sizeof(private_message));
                        int nz = snprintf(private_message, sizeof(private_message), ">>   --> %s...", clients[i].name);

                        n = write(clients[index].socket, private_message, strlen(private_message));
                        if(n < 0) perror("ERROR writing to socket");
                    }
                }
                continue;
            }

            // getting the current time 
            char timestamp[30];
            bzero(timestamp, sizeof(timestamp));
            getTimeStamp(timestamp, sizeof(timestamp));

            bool found = false;
            // looping through all client and matching the name
            for(int i=0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != 0 && !strcmp(clients[i].name, target_name)) {
                    // adding the sender name, timestamp and message to message to be sent 

                    char private_message[BUFFER_SIZE + 50 + 30];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), "%s%s : %s", timestamp, clients[index].name, message);
                    //nz stores the final size of the message, to remove warning

                    // sending the private message to target client
                    n = write(clients[i].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                    found = true;
                    break;
                }
            }
            if(!found) {
                // if target client is not found
                char private_message[BUFFER_SIZE + 50 + 30];
                bzero(private_message, sizeof(private_message));
                int nz = snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", target_name);

                n = write(clients[index].socket, private_message, strlen(private_message));
                if(n < 0) perror("ERROR writing to socket");
            }
        }
        else if (buffer[0] == '#') {
            // REPORTING
            char reported[50];
            sscanf(buffer, "#%s", reported);

            int found = 0;
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != 0 && !strcmp(clients[i].name, reported)) {
                    found = 1;
                    if(!clients[i].report[index]) {
                        found = 2;
                        clients[i].report[index] = 1;
                        reportCheck(i);

                        char private_message[BUFFER_SIZE];
                        bzero(private_message, sizeof(private_message));
                        int nz = snprintf(private_message, sizeof(private_message), ">> %s HAS BEEN REPORTED.", reported);

                        n = write(clients[index].socket, private_message, strlen(private_message));
                        if(n < 0) perror("ERROR writing to socket");
                    }
                    break;
                }
            }
            if(found == 0) {
                char private_message[BUFFER_SIZE];
                bzero(private_message, sizeof(private_message));
                int nz = snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", reported);

                n = write(clients[index].socket, private_message, strlen(private_message));
                if(n < 0) perror("ERROR writing to socket");
            }
            if(found == 1){
                char private_message[BUFFER_SIZE];
                bzero(private_message, sizeof(private_message));
                int nz = snprintf(private_message, sizeof(private_message), ">> %s HAS ALREADY BEEN REPORTED BY YOU...", reported);

                n = write(clients[index].socket, private_message, strlen(private_message));
                if(n < 0) perror("ERROR writing to socket");
            }
        }

        //GROUP CHAT FUNCTIONALITY STARTS HERE
        else if(buffer[0]=='$'){
            // printf("HELLO TO GROUP SECTION\n");
            char group_name[50], message[BUFFER_SIZE];
            sscanf(buffer, "$%s %[^\n]", group_name, message);

            if(strcmp(group_name,"SHOW_ALL")==0){
                // SHOWALL command to show all the groups connected to the server
                char private_message[BUFFER_SIZE];
                bzero(private_message, sizeof(private_message));
                int nz = snprintf(private_message, sizeof(private_message), ">> GROUPS IN THE SERVER...");

                n = write(clients[index].socket, private_message, strlen(private_message));
                if(n < 0) perror("ERROR writing to socket");

                for(int i=0; i < MAX_GROUPS; i++) {
                    if(groups[i].groupID != 0) {
                        bzero(private_message, sizeof(private_message));
                        int nz = snprintf(private_message, sizeof(private_message), ">>    --> %s...", groups[i].groupName);

                        n = write(clients[index].socket, private_message, strlen(private_message));
                        if(n < 0) perror("ERROR writing to socket");
                    }
                }
                continue;
            }
            if(strcmp(group_name,"CREATE")==0){
                // message acts as group name here
                // create format "$CREATE <groupName>"
                int group_avail = 0;
                for(int i=0;i<MAX_GROUPS;i++){
                    if(groups[i].groupID!=0 && strcmp(groups[i].groupName, message)==0){
                        group_avail =2;
                    }
                }
                for(int i = 0; i<MAX_GROUPS; i++){
                    if(group_avail==2) break;
                    if(groups[i].groupID==0){
                        // availabe socket for group found
                        group_avail=1;
                        groups[i].groupID=i+1;
                        
                        strncpy(groups[i].groupName, message, sizeof(groups[i].groupName) - 1);
                        groups[i].groupName[sizeof(groups[i].groupName)-1] = '\0';

                        memset(groups[i].indexNumbers, -1, sizeof(groups[i].indexNumbers));
                        groups[i].indexNumbers[0]=index;
                        
                        char private_message[BUFFER_SIZE];
                        bzero(private_message, sizeof(private_message));
                        int nz = snprintf(private_message, sizeof(private_message), ">> %s GROUP CREATED SUCCESSFULLY...", groups[i].groupName);

                        n = write(clients[index].socket, private_message, strlen(private_message));
                        if(n < 0) perror("ERROR writing to socket");

                        break;
                    }
                }
                if(group_avail==0){
                    // maximum group limit by server reached
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> MAXIMUM NO OF GROUPS REACHED...");

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
                else if(group_avail==2){
                    // groupName already in use
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> %s GROUP ALREADY EXISTS, JOIN %s GROUP...",message,message);

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
            }
            else if(strcmp(group_name,"JOIN")==0){
                // message acts as group name here
                // create format "$JOIN <groupName>"
                int group_found= 0;
                for(int i = 0; i < MAX_GROUPS; i++) {
                    // printf("%s  %s\n", groups[i].groupName,message);
                    if(groups[i].groupID != 0 && strcmp(groups[i].groupName, message)==0){
                        group_found=1;

                        for(int j=0;j<MAX_CLIENTS;j++){
                            if(groups[i].indexNumbers[j]==index){
                                group_found=2;
                                break;
                            }
                        }
                        if(group_found==2) break;

                        int z=0;
                        for(z;z<MAX_CLIENTS;z++) if(groups[i].indexNumbers[z]==-1) break;
                        groups[i].indexNumbers[z]=index;

                        char private_message[BUFFER_SIZE];
                        bzero(private_message, sizeof(private_message));
                        int nz = snprintf(private_message, sizeof(private_message), ">> %s JOINED THE GROUP %s SUCCESSFULLY...", clients[index].name, groups[i].groupName);

                        n = write(clients[index].socket, private_message, strlen(private_message));
                        if(n < 0) perror("ERROR writing to socket");
                        break;
                    }
                }
                if(group_found == 0) {
                    // group not found
                    // message acts as group name here
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", message);

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
                if(group_found == 2){
                    // already in the group
                    // message acts as group name here
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> YOU ARE ALREADY IN %s GROUP ...",message);

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
            }
            else if(strcmp(group_name,"LEAVE")==0){
                //message act as group_name here
                // create format "$LEAVE <groupName>"
                int found_in_group=0;
                for(int i=0;i<MAX_GROUPS;i++){
                    if(groups[i].groupID!=0 && strcmp(groups[i].groupName,message)==0){
                        // found the group with given groupName
                        found_in_group=1;
                        for(int j=0;j<MAX_CLIENTS;j++){
                            // removing the client from groupName if client is member of group
                            found_in_group=2;
                            if(groups[i].indexNumbers[j]==index){
                                groups[i].indexNumbers[j]=-1;

                                char private_message[BUFFER_SIZE];
                                bzero(private_message, sizeof(private_message));
                                int nz = snprintf(private_message, sizeof(private_message), ">> %s LEFT THE GROUP %s SUCCESSFULLY...", clients[index].name, groups[i].groupName);

                                n = write(clients[index].socket, private_message, strlen(private_message));
                                if(n < 0) perror("ERROR writing to socket");
                                break;
                            }
                        }
                    }
                }
                if(found_in_group==0){
                    // no group with given groupName
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", message);

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
                else if(found_in_group==1){
                    // groupname exits but client is not member of group
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> YOU ARE NOT IN %s GROUP...", message);

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
            }
            else{
                // here group_name will act as group name only...
                // create format "$groupName <message>"
                int group_found=0;
                for(int i=0;i<MAX_GROUPS;i++){
                    if(groups[i].groupID!=0 && strcmp(groups[i].groupName,group_name)==0){
                        group_found=1;
                        for(int j=0;j<MAX_CLIENTS;j++){
                            if(groups[i].indexNumbers[j]==index){
                                group_found=2;
                            }
                        }
                        if(group_found==1) break;
                        for(int j=0;j<MAX_CLIENTS;j++){
                            if(groups[i].indexNumbers[j]!=-1 && groups[i].indexNumbers[j]!=index){
                                // sending message to all members in the group
                                char timestamp[30];
                                bzero(timestamp, sizeof(timestamp));
                                getTimeStamp(timestamp, sizeof(timestamp));

                                char private_message[BUFFER_SIZE + 50 + 30];
                                bzero(private_message, sizeof(private_message));
                                int nz = snprintf(private_message, sizeof(private_message), "[%s]%s%s : %s", group_name,timestamp, clients[index].name, message);

                                if(clients[groups[i].indexNumbers[j]].socket != 0) {
                                    n = write(clients[groups[i].indexNumbers[j]].socket, private_message, strlen(private_message));
                                    if(n < 0) perror("ERROR writing to socket");
                                }
                            }
                        }
                    }
                }
                if(group_found==0){
                    // group with given groupName not exits 
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", group_name);

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
                if(group_found==1){
                    // group exits but client is not member
                    char private_message[BUFFER_SIZE];
                    bzero(private_message, sizeof(private_message));
                    int nz = snprintf(private_message, sizeof(private_message), ">> YOU ARE NOT IN %s GROUP, FIRST JOIN THE GROUP %s...", group_name,group_name);

                    n = write(clients[index].socket, private_message, strlen(private_message));
                    if(n < 0) perror("ERROR writing to socket");
                }
            }
            // printf("%s",group_name);
            // printf("%s",message);
        }
        //GROUP CHAT FUNCTIONALITY ENDS HERE
        
        else{
            // --BROADCASTING--
            // message format "<message>"

            // getting the current time 
            char timestamp[30];
            bzero(timestamp, sizeof(timestamp));
            getTimeStamp(timestamp, sizeof(timestamp));

            // printf("%s", timestamp);

            // adding the sender name, timestamp and message to message to be sent 
            char message[BUFFER_SIZE + 50 + 30];
            bzero(message, sizeof(message));
            int nz = snprintf(message, sizeof(message), "[BROADCASTING]%s%s : %s", timestamp, clients[index].name, buffer);
            //nz stores the final size of the message 

            // broadcasting the message to all client except the sender
            for(int i=0; i < MAX_CLIENTS; i++) {
                if(i != index && clients[i].socket != 0) {
                    n = write(clients[i].socket, message, strlen(message));
                    if(n < 0) perror("ERROR writing to socket");
                }
            }
        }
    }

    // closing the socket and the thread
    close(clients[index].socket);
    pthread_exit(NULL);
}

void *server_thread(void *arg){
    // server thread to get command from server
    // currently only for focefully removing a client and closing the server
    // message format "REMOVE username" for removing a client
    // message format "CLOSE" for closing the server
    // message format "DELETE groupName" for deleting the group

    char buffer[BUFFER_SIZE];
    ssize_t n;

    while(true) {
        bzero(buffer, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE - 1, stdin);
        buffer[strcspn(buffer, "\n")] = 0;
        fflush(stdout);  

        if (strcmp(buffer, "CLOSE") == 0) {
            printf("BYE\n");
            _exit(EXIT_FAILURE);
        }

        char target[50], name[50];
        sscanf(buffer, "%s %[^\n]", target, name);
        printf("%s %s\n", target, name);

        if(strcmp(target, "REMOVE") == 0) {
            // serching the client using username and kicking the client out
            bool found = false;
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != 0 && !strcmp(clients[i].name, name)) {
                    char message[BUFFER_SIZE];
                    int nz = snprintf(message, sizeof(message), ">> Kicked Out...");
                    write(clients[i].socket, message, strlen(message)); 

                    close(clients[i].socket);
                    clients[i].socket = 0;
                    found = true;
                    fflush(stdin);
                    break;
                }
            }
            if(!found) {
                printf("%s NOT FOUND IN THE CHAT...\n", name);
                fflush(stdin);
            }     
        }

        if(strcmp(target, "DELETE") == 0) {
            // serching the group using groupName and deleting the group
            bool found = false;
            for(int i = 0; i < MAX_GROUPS; i++) {
                if(groups[i].groupID != 0 && !strcmp(groups[i].groupName, name)) {
                    found = true;
                    for(int j=0;j<MAX_CLIENTS;j++){
                        if(groups[i].indexNumbers[j]!=-1){
                            if(clients[groups[i].indexNumbers[j]].socket != 0) {
                                char message[BUFFER_SIZE];
                                int nz = snprintf(message, sizeof(message), ">> %s GROUP DELETED BY THE SERVER...",groups[i].groupName);
                                write(clients[groups[i].indexNumbers[j]].socket, message, strlen(message)); 
                            }
                            groups[i].indexNumbers[j]=-1;
                        }
                    }
                    bzero(groups[i].groupName, sizeof(groups[i].groupName));
                    groups[i].groupID=0;
                    printf("%s GROUP DELETED SUCCESSFULLY...\n", name);
                    fflush(stdin);
                    break;
                }
            }
            if(!found) {
                printf("%s NOT FOUND IN THE CHAT...\n", name);
                fflush(stdin);
            }     
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

    if(argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]); // Assigning port
    
    // STEP 1: socket()
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) error("ERROR opening socket");
    
    // prepare the sockaddr_in structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    // STEP 3: bind() and listen()
    if(bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) error("ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    printf("Server Started at port %d\n", portno);

    // thread for listening to the server commands
    pthread_t serverThread;
    if(pthread_create(&serverThread, NULL, server_thread, NULL) < 0) {
        perror("Error in creating thread");
        close(sockfd);
    }
    pthread_detach(serverThread);

    pthread_t timeoutThread;
    if(pthread_create(&timeoutThread, NULL, timeout_checker, NULL) < 0) {
        perror("Error in creating thread");
        close(sockfd);
    }
    pthread_detach(timeoutThread);

    while(true) {    
        int* newsockfd = malloc(sizeof(int));

        *newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if(*newsockfd < 0){
            error("ERROR on accept");
            close(*newsockfd);
            free(newsockfd);
            continue;
        }

        // assinging the free socket to the incomming client
        int client_avail = 0;
        for(int i = 0; i < MAX_CLIENTS; i++) {
            if(clients[i].socket == 0){
                client_avail = 1;
                clients[i].socket = *newsockfd;

                pthread_t client_thread;
                int* client_index = malloc(sizeof(int));
                *client_index = i;
                
                // creating a new thread to listen to the client requests
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
            // denying the join request due to socket unavailability
            n = write(*newsockfd, "MAXIMUM NO. OF CLIENTS REACHED !!!", 34);
            if(n < 0) error("ERROR writing to socket");
            close(*newsockfd);
            free(newsockfd);
        }
    }

    close(sockfd);
    return 0;
}