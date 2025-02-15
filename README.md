# ChatRoom in C

## OverView
ChatRoom in C with Socket Programming using threads and using selectAll. Messages transfer are end to end encrypted. No one outside of the chat, not even server, can read or listen to them.

## Table of Contents
- [Overview](#overview)
- [Using Threads](#using-threads)
- [Using SelectAll](#using-selectall)
- [Features](#features)
- [Commands Supported](#commands-supported)
- [Contributors](#contributors)

## Using Threads
serverThread.c and clientThread.c uses threads to achieve Client Server Architecture.

Command to start serverThread.c

  ```bash
  gcc serverThread.c -o serverThread
  ./serverThread <port_no>
  ```
<br>
Command to start clientThread.c

  ```bash
    gcc clientThread.c -o clientThread
    ./clientThread <IP_Address> <port_no> <username>
  ```

## Using SelectAll
server.c and client.c uses selectall() system call to achieve Client Server Architecture.

Command to start server.c

  ```bash
  gcc server.c -o server
  ./server <port_no>
  ```
<br>
Command to start client.c

  ```bash
    gcc client.c -o client
    ./client <IP_Address> <port_no> <username>
  ```

## Features
- Handling Multiple Client
- **Unique** Client can join and disconnect
- Private Message between two Clients
- Broadcasting to all Alive Clients
- File transfer between two Clients
- Reporting a Client using username
- Client removal power to server
- Messages and files are **end-to-end encrypted**
- Time-out after 60 seconds (subjected to change)
- Group chats for multiple select clients supported
- Features of group chats:
  - Create a group
  - Join a group
  - Leave a group
- Groups with no members automatically gets deleted
 
## Commands Supported 
### Client side
- `<message>` - for direct broadcasting the message to all client
- `@username <message>` - enter the username of client to send the private message
- `@username @file <fileName>` - to send file fileName
- `#username` - to report a sus client with username
- `EXIT` - to disconnect from the chat server

#### Group Chat
- `$CREATE <groupName>` - to create a group
- `$JOIN <groupName>` - to join a group
- `$LEAVE <groupName>` - to leave a group
- `$groupName <message>` - to send message in the group

These commands are direct requests to server, hence they are not encrypted.

### Server Side
- `REMOVE <username>` - to kick out client with username
- `CLOSE` - to close the server

## Contributors
- **Saurav Kumar Singh (22CS01010)**
- **Suprit Naik (22CS01018)**
- **Om Prakash Behera (22CS01041)**
- **Harsh Maurya (22CS01046)**
