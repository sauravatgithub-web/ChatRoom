# ChatRoom in C

## OverView
ChatRoom in C with Socket Programming using threads and using selectAll. Messages transfer are end to end encrypted. No one outside of the chat, not even server, can read or listen to them.<br> 
## Table of Contents
- [Overview](#overview)
- [Using Threads](#using-threads)
- [Using SelectAll](#using-selectall)
- [Features](features)
- [Commands Supported](commands-supported)

## Using Threads
serverThread.c and clientThread.c uses selectall() system call to achieve Client Server Architecture.<br>
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
server.c and client.c use selectall() system call to achieve Client Server Architecture.<br>
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
- <b>Unique</b> Client can join and disconnect
- Private Message between two Clients
- Broadcasting to all Alive Clients
- File transfer between two Clients
- Reporting a Client using username
- Client removal power to server
- Messages and files are <b>end-to-end encrypted</b>
 
## Commands Supported 
### Client side 
- `<message>` - for direct broadcasting the message to all client
- `@username <message>` - enter the username of client to send the private message
- `@username @file <fileName>` - to send file fileName
- `#username` - to report a sus client with username
  <br>
### Server Side
- `REMOVE username` - to kick out client with username
  
