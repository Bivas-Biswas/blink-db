#ifndef SET_NONBLOCKING_UTILS_H
#define SET_NONBLOCKING_UTILS_H

#include <iostream>
#include <fcntl.h>

/**
 * @brief Sets a socket to non-blocking mode.
 *
 * This function retrieves the current file status flags of the socket,
 * adds the O_NONBLOCK flag, and updates the socket settings.
 *
 * @param sock The socket file descriptor.
 */
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

#endif // SET_NONBLOCKING_UTILS_H