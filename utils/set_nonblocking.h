#ifndef set_nonblocking_utils_h
#define set_nonblocking_utils_h

#include <iostream>
#include <fcntl.h>

// Set socket to non-blocking mode
void set_nonblocking(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL failed");
        exit(EXIT_FAILURE);
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL failed");
        exit(EXIT_FAILURE);
    }
}

#endif