# Client Server Process
## Overview
This project extends the IPC capabilities of a client-server implementation using the TCP/IP protocol. The client and server programs are modified to handle data point and file transfers across the network using TCP/IP request channels.

## Features
- *TCP/IP Communication*: Facilitates remote network communication between client and server.
- *Multithreading*: Server can handle multiple client requests simultaneously.
- *Data Point Transfers*: Supports the transfer of data points.
- *File Transfers*: Allows file transfer operations between client and server.
## Technologies
- *C++*: Primary programming language.
- *TCP/IP Protocol*: Used for network communication.
- *Multithreading*: For handling multiple client connections.
## Installation
To set up the Client Server Process on your local system, follow these steps:

```bash
git clone https://github.com/SamiMelhem/Client-Server-Process.git
cd Client-Server-Process
make
```
## Usage
Run the server using the following command:

```bash
./server -r <port number>
```
Run the client using the following command:

```bash
./client -a <IP address> -r <port number> [additional options]
```
## File Descriptions
- *server.cpp*: Contains the implementation of the server.
- *client.cpp*: Contains the implementation of the client.
- *TCPRequestChannel.cpp*: Handles TCP/IP communication.
