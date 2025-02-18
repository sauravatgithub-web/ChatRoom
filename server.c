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

#define TIMEOUT 1000
#define MAX_GROUPS 10
#define MAX_CLIENTS 10
#define BUFFER_SIZE 256
#define EXIT_KEYWORD "EXIT"
#define SHOW_GROUPS "SHOW_ALL_GROUPS"
#define SHOW_CLIENTS "SHOW_ALL_CLIENTS"

typedef struct {
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

    for(int i = 0; i < MAX_GROUPS; i++) {
        if(groups[i].groupID) {
            int available = 0;
            for(int j = 0; j < MAX_CLIENTS; j++) {
                if(groups[i].indexNumbers[j] != -1) {
                    available = 1;
                    break;
                }
            }

            if(!available) {
                memset(groups[i].groupName, 0, sizeof(groups[i].groupName));
                groups[i].groupID = 0;
            }
        }
    }
}

void remove_client_from_every_group(int client_index) {
    for(int i = 0; i < MAX_GROUPS; i++) {
        for(int j = 0; j < MAX_CLIENTS; j++) {
            if(groups[i].indexNumbers[j] == client_index) groups[i].indexNumbers[j] = -1;
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

                        bool found = false;
                        for(int z = 0; z < MAX_CLIENTS; z++) {
                            if(clients[z].socket != 0 && !strcmp(clients[z].name, buffer)) {
                                found = true;
                                break;
                            }
                        }
                        if(found) {
                            send(newsockfd, ">> User with same name already exists.", 40, 0);
                            close(newsockfd);
                            FD_CLR(newsockfd, &master_fds);
                            continue;
                        }

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

                    char target_name[50];

                    if(!strncmp(buffer, "DELETE", 6)) {
                        // extracting the target  
                        memset(target_name, 0, sizeof(target_name));
                        sscanf(buffer, "DELETE %s", target_name);

                        // searching the group using groupName and deleting it
                        bool found = false;
                        for(int i = 0; i < MAX_GROUPS; i++) {
                            if(groups[i].groupID != 0 && !strcmp(groups[i].groupName, target_name)) {
                                found = true;

                                for(int j = 0; j < MAX_CLIENTS; j++) {
                                    if(groups[i].indexNumbers[j] != -1) {
                                        if(clients[groups[i].indexNumbers[j]].socket) {
                                            char message[BUFFER_SIZE];
                                            snprintf(message, sizeof(message), ">> GROUP DELETED BY THE SERVER...");
                                            send(clients[groups[i].indexNumbers[j]].socket, message, strlen(message), 0); 
                                        }
                                        groups[i].indexNumbers[j] =- 1;
                                    }
                                }

                                memset(groups[i].groupName, 0, sizeof(groups[i].groupName));
                                groups[i].groupID = 0;
                                printf("%s GROUP DELETED SUCCESSFULLY...\n", target_name);
                                fflush(stdin);
                                break;
                            }
                        }
                        if(!found) {
                            printf("%s NOT FOUND IN THE CHAT...\n", target_name);
                            fflush(stdin);
                        }     
                    }

                    if(!strncmp(buffer, "REMOVE", 6)) {
                        // extracting the target  
                        memset(target_name, 0, sizeof(target_name));
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
                        int index = get_client(sender_name);
                        clients[index].last_active = time(NULL);
                        // printf("%d ", index);
                        
                        char timestamp[30];
                        bzero(timestamp, sizeof(timestamp));
                        getTimeStamp(timestamp, sizeof(timestamp));   // Retrieving message time

                        if(!strcmp(buffer, EXIT_KEYWORD)) {           // Handling client request to exit
                            remove_client_from_every_group(i);
                            remove_client_from_list(i);
                            FD_CLR(i, &master_fds);
                            continue;
                        }

                        if(!strcmp(buffer, SHOW_CLIENTS)) {            // Handling client request to show all clients
                            char private_message[BUFFER_SIZE];
                            memset(private_message, 0, BUFFER_SIZE);
                            snprintf(private_message, sizeof(private_message), ">> ALIVE CLIENTS AT THIS MOMENT....");
                            send(i, private_message, strlen(private_message), 0);

                            for(int k = 0; k < MAX_CLIENTS; k++) {
                                if(clients[k].socket) {
                                    // printf("%d ", k);
                                    memset(private_message, 0, BUFFER_SIZE);
                                    snprintf(private_message, sizeof(private_message), ">>   --> %s...", clients[k].name);
                                    send(i, private_message, strlen(private_message), 0);
                                }
                            }
                            continue;
                        }

                        if(!strcmp(buffer, SHOW_GROUPS)) {             // Handling client request to show all groups
                            char private_message[BUFFER_SIZE];
                            memset(private_message, 0, BUFFER_SIZE);
                            snprintf(private_message, sizeof(private_message), ">> ALIVE GROUPS AT THIS MOMENT....");
                            send(clients[index].socket, private_message, strlen(private_message), 0);

                            for(int k = 0; k < MAX_GROUPS; k++) {
                                if(groups[k].groupID) {
                                    memset(private_message, 0, BUFFER_SIZE);
                                    snprintf(private_message, sizeof(private_message), ">>   --> %s...", groups[k].groupName);
                                    send(i, private_message, strlen(private_message), 0);
                                }
                            }
                            continue;
                        }

                        // PRIVATE OR GROUP MESSAGING
                        // private message format "@username <message>"
                        // group message format "@groupname <message>"
                        if(strncmp(buffer, "@", 1) == 0) {                        // PRIVATE MESSAGING
                            char recipient[50], msg[BUFFER_SIZE];
                            sscanf(buffer, "@%s %[^\n]", recipient, msg);
                            int recipient_socket = find_client_socket(recipient); // Retrieving recipient socket

                            if(recipient_socket != -1) {
                                memset(message, 0, sizeof(message));
                                snprintf(message, sizeof(message), "%s%s : %s", timestamp, sender_name, msg);
                                send(recipient_socket, message, strnlen(message, sizeof(message)), 0);  // Writing to the client
                            } 
                            else { // User not found
                                send(i, ">> User not found.\n", 20, 0);
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
                        else if(strncmp(buffer, "$", 1) == 0) {                       // GROUP MESSAGING
                            char group_name[50], message[BUFFER_SIZE], private_message[2*BUFFER_SIZE];
                            sscanf(buffer, "$%s %[^\n]", group_name, message);
 
                            if(!strcasecmp(group_name, "CREATE")) {                   // CREATE GROUP
                                // format <$CREATE groupName>
                                int group_avail = 0;
                                for(int i = 0; i < MAX_GROUPS; i++) { // Checking for identical group name
                                    if(groups[i].groupID && !strcmp(groups[i].groupName, message)) {
                                        group_avail = 2;
                                    }
                                }
                                for(int i = 0; i < MAX_GROUPS; i++) {
                                    if(group_avail == 2) break;
                                    if(!groups[i].groupID) {                // Successfully creating a group
                                        group_avail = 1;
                                        groups[i].groupID = i + 1;
                                        
                                        strncpy(groups[i].groupName, message, sizeof(groups[i].groupName) - 1);
                                        groups[i].groupName[sizeof(groups[i].groupName)-1] = '\0';

                                        memset(groups[i].indexNumbers, -1, sizeof(groups[i].indexNumbers));
                                        groups[i].indexNumbers[0] = index;
                                        
                                        memset(private_message, 0, sizeof(private_message));
                                        snprintf(private_message, sizeof(private_message), ">> %s GROUP CREATED SUCCESSFULLY...", groups[i].groupName);
                                        send(clients[index].socket, private_message, strlen(private_message), 0); // Sending confirmation to client
                                        break;
                                    }
                                }
                                if(!group_avail) { // maximum group limit by server reached
                                    memset(private_message, 0, sizeof(private_message));
                                    snprintf(private_message, sizeof(private_message), ">> MAXIMUM NO OF GROUPS REACHED...");
                                    send(clients[index].socket, private_message, strlen(private_message), 0);
                                }
                                else if(group_avail == 2) { // group name already in use
                                    memset(private_message, 0, sizeof(private_message));
                                    snprintf(private_message, sizeof(private_message), ">> %.100s GROUP ALREADY EXISTS, JOIN %.100s GROUP...", message, message);
                                    send(clients[index].socket, private_message, strlen(private_message), 0);
                                }
                            }
                            else if(!strcasecmp(group_name, "JOIN")) {                // JOIN GROUP
                                // format <$JOIN groupName>
                                int group_found = 0;
                                for(int i = 0; i < MAX_GROUPS; i++) {
                                    if(groups[i].groupID && !strcmp(groups[i].groupName, message)) {  // Searching and joining a group
                                        group_found = 1;

                                        for(int j = 0; j < MAX_CLIENTS; j++) {
                                            if(groups[i].indexNumbers[j] == index) {
                                                group_found = 2;
                                                break;
                                            }
                                        }
                                        if(group_found == 2) break;

                                        int reqd_index = 0;
                                        while(reqd_index < MAX_CLIENTS) {
                                            if(groups[i].indexNumbers[reqd_index] == -1) break;
                                            reqd_index++;
                                        }
                                        groups[i].indexNumbers[reqd_index] = index;
                
                                        memset(private_message, 0, sizeof(private_message));
                                        snprintf(private_message, sizeof(private_message), ">> %s JOINED THE GROUP %s SUCCESSFULLY...", clients[index].name, groups[i].groupName);
                                        send(clients[index].socket, private_message, strlen(private_message), 0); // Sending confirmation to client
                                        break;
                                    }
                                }
                                if(!group_found) {                      // Group requested for a non existent group
                                    memset(private_message, 0, sizeof(private_message));
                                    snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", message);
                                    send(clients[index].socket, private_message, strlen(private_message), 0);
                                }
                                if(group_found == 2) {                  // Client already in the group
                                    memset(private_message, 0, sizeof(private_message));
                                    snprintf(private_message, sizeof(private_message), ">> YOU ARE ALREADY IN %s GROUP ...", message);
                                    send(clients[index].socket, private_message, strlen(private_message), 0);
                                }
                            }
                            else if(!strcasecmp(group_name, "LEAVE")) {                      // LEAVE GROUP
                                // format <$LEAVE groupName>
                                int found_in_group = 0;
                                for(int i = 0; i < MAX_GROUPS; i++) {
                                    if(groups[i].groupID && !strcmp(groups[i].groupName, message)) {
                                        found_in_group = 1;                                  // Group found
                                        for(int j = 0; j < MAX_CLIENTS; j++) {
                                            found_in_group = 2;                              // Client left the group successfully
                                            if(groups[i].indexNumbers[j] == index) {
                                                groups[i].indexNumbers[j] = -1;              
                
                                                memset(private_message, 0, sizeof(private_message));
                                                snprintf(private_message, sizeof(private_message), ">> %s LEFT THE GROUP %s SUCCESSFULLY...", clients[index].name, groups[i].groupName);
                                                send(clients[index].socket, private_message, strlen(private_message), 0);  // Sending confirmation to client
                                                break;
                                            }
                                        }
                                    }
                                }
                                if(found_in_group == 0 || found_in_group == 1) {   // Non existent group or Client tried to leave a group in which he is not there
                                    memset(private_message, 0, sizeof(private_message));
                                    if(found_in_group == 0) snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", message);
                                    else snprintf(private_message, sizeof(private_message), ">> YOU ARE NOT IN %s GROUP...", message);

                                    send(clients[index].socket, private_message, strlen(private_message), 0);
                                }
                            }
                            else {                                                   // GROUP MESSAGING
                                // message format <$groupName message>
                                int group_found = 0;
                                for(int p = 0; p < MAX_GROUPS; p++) {
                                    if(groups[p].groupID && !strcmp(groups[p].groupName, group_name)) {    // Searching for the group
                                        group_found = 1;
                                        for(int j = 0; j < MAX_CLIENTS; j++) {
                                            if(groups[p].indexNumbers[j] == index) {
                                                group_found = 2;
                                                break;
                                            }
                                        }
                                        if(group_found == 1) break;

                                        for(int j = 0; j < MAX_CLIENTS; j++) {          // Sending messages to all members in the group
                                            if(groups[p].indexNumbers[j] != index && groups[p].indexNumbers[j] != -1) {
                                                memset(private_message, 0, sizeof(private_message));
                                                snprintf(private_message, sizeof(private_message), "[%s]%s%s : %s", group_name, timestamp, clients[index].name, message);

                                                if(clients[groups[p].indexNumbers[j]].socket != 0) {
                                                    send(clients[groups[p].indexNumbers[j]].socket, private_message, strlen(private_message), 0);
                                                }
                                            }
                                        }
                                    }
                                }
                                if(group_found == 0 || group_found == 1) {   // Group not found or Client is not in the group
                                    memset(private_message, 0, sizeof(private_message));
                                    if(group_found == 0) snprintf(private_message, sizeof(private_message), ">> %s NOT FOUND...", group_name);
                                    else snprintf(private_message, sizeof(private_message), ">> YOU ARE NOT IN %s GROUP, FIRST JOIN THE GROUP %s...", group_name,group_name);

                                    send(clients[index].socket, private_message, strlen(private_message), 0);
                                }
                            }
                        }
                        else {
                            // BROADCASTING TO ALL ALIVE USERS
                            // format "message"

                            memset(message, 0, sizeof(message));
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