#include "TCPRequestChannel.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
// #include <iostream>
#include <cstdlib>

using namespace std;


TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    // if (_ip_address == "") // for the server
    // {
    //     // set up variables 
    //     struct sockaddr_in server;
    //     int server_socket, bind_stat, listen_stat;

    //     // socket - make socket - socket(int domain, int type, int protocol)
    //     // AF_INET = IPv4
    //     // SOCK_STREAM = TCP
    //     // Normally only a singl protocol exists to support a particular socket type
    //     // within a given protocol family, in which protocal can be specified as 0.
    //     // sockfd = socket(AF_INET, SOCK_STREAM, 0);

    //     // provide necessary machine info for sockaddr_in
    //     // address family, IPv4
    //     // IPv4 address, use current IPv4 address (INADDR_ANY)
    //     // connection port
    //     // convert short from host byte order to network byte order

    //     // bind - assign address to socket - bind(int sockfd, const struct sockaddr *addr, scoklen_t addrlen)

    //     // listen - listen for client - listen(int sockfd, int backlog)

    //     // accept - accept connection
    //     // written in a seperate method
    // }
    // else // for the client
    // {
    //     // set up variables
    //     struct sockaddr_in server_info;
    //     int client_sock, connect_stat;

    //     // socket - make socket - socket(int domain, int type, int protocol)

    //     // generate server's info based on parameters
    //     // address family, IPv4
    //     // connection port
    //     // convert short from host byte order to network byte order
    //     // convert ip address c-stringto binary representation for sin_addr

    //     // connect - connect to listening socket - connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    // }
    struct sockaddr_in address;
    uint16_t port = atoi(_port_no.c_str());

    memset(&address, 0, sizeof(address));


    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    if (_ip_address.empty()){ // First steps for setting up the server
        address.sin_addr.s_addr = INADDR_ANY;

        // int yes = 1; // Enable the reuse option
        // if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        //     perror("setsockopt(SO_REUSEADDR) failed");
        //     exit(EXIT_FAILURE);
        // }

        if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) { 
            perror("Bind failed.");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        if (listen(sockfd, 10) < 0){ // Backlog of 10 connections
            perror("Listen failed.");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    } else { // Client setup section once the server is established
        address.sin_addr.s_addr = inet_addr(_ip_address.c_str());

        if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("Connect failed.");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) : sockfd(_sockfd) { }

TCPRequestChannel::~TCPRequestChannel () { close(this->sockfd); }

int TCPRequestChannel::accept_conn () {
    // accept - accept connection
    // socket file descriptor for accepted connection
    // accept connection
    // return socket file descriptor

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
    if (new_sockfd < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    return new_sockfd;
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    ssize_t no_bytes = read(sockfd, msgbuf, msgsize); // number of bytes to read
    // read from socket - read(int fd, void *buf, size_t count)
    // return number of bytes read
    // if (no_bytes < 0) {
    //     perror("Reading from socket failed");
    // }
    return no_bytes;
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    ssize_t no_bytes = write(sockfd, msgbuf, msgsize); // number of bytes to write
    // write to socket - write(int fd, const void *buf, size_t count)
    // return number of byte written
    // if (no_bytes < 0) {
    //     perror("Writing to socket failed");
    // }
    
    return no_bytes; 
}
