#ifndef create_non_locking_socket_utils_h
#define create_non_locking_socket_utils_h

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "./set_nonblocking.h"

/**
 * @brief Creates a non-blocking socket.
 * @param ip The ip to bind the socket to.
 * @param port The port number to bind the socket to.
 * @param addr The addr to save the socket address.
 * @return The socket file descriptor.
 */
int create_non_locking_socket(const std::string ip, const int port, struct sockaddr_in& addr)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));     

    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    set_nonblocking(sockfd);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, SOMAXCONN) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

#endif