/**
 * @file load_balancer.h
 * @brief Implements a non-blocking load balancer using epoll and consistent hashing.
 */
#ifndef LOAD_BALANCER_BLINK_H
#define LOAD_BALANCER_BLINK_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "../utils/create_non_locking_socket.h"
#include "../utils/set_nonblocking.h"
#include "./types.h"

/**
 * @class LoadBalancer
 * @brief Handles load balancing by distributing client requests to backend servers.
 */
class LoadBalancer
{
private:
    /**
     * @typedef parsingKeyFuncPtr
     * @brief Function pointer type for key parsing function.
     */
    using parsingKeyFuncPtr = std::string (*)(char *buffer, int buffer_len);

    std::set<int> hash_ring_;                       ///< Hash ring for consistent hashing.
    std::unordered_map<int, ServerAdd> server_map_; ///< Maps hash values to server ports.
    std::vector<ServerAdd> servers_add_;            ///< Stores server addresses.
    int lb_port_ = -1;                              ///< Load balancer port.
    std::string lb_ip_;                             ///< Load balancer ip.
    int max_events_ = 100;                          ///< Maximum number of epoll events.
    int buffer_size_ = 1024;                        ///< Buffer size for receiving data.
    char *buffer_ = nullptr;                        ///< Buffer for storing client requests.

    /**
     * @brief Gets the nearest server from the hash ring.
     * @param key The key used to determine the server.
     * @return The port of the selected server.
     */
    ServerAdd getServer(const std::string &key);

    /**
     * @brief Hash function to normalize hash space.
     * @param key The key to hash.
     * @return The hashed value.
     */
    int hashKey(const std::string &key);

public:
    /**
     * @brief Constructs a LoadBalancer instance.
     * @param lb_port The port the load balancer listens on.
     * @param serves_add The list of server addresses and ports.
     * @param buffer_size The size of the buffer for client messages.
     * @param max_events The maximum number of events handled by epoll.
     */
    LoadBalancer(std::string lb_ip, int lb_port, std::vector<ServerAdd> &servers_add, int buffer_size, int max_events);

    /**
     * @brief Destructor to clean up resources.
     */
    ~LoadBalancer();

    /**
     * @brief Initializes the server and starts handling client requests.
     * @param func Function pointer for parsing the key from client messages.
     */
    void server_init(parsingKeyFuncPtr func);
};

LoadBalancer::LoadBalancer(std::string lb_ip, int lb_port, std::vector<ServerAdd> &servers_add, int buffer_size, int max_events) : servers_add_(servers_add)
{
    lb_ip_ = lb_ip;
    lb_port_ = lb_port;
    buffer_size_= buffer_size;
    max_events_= max_events;

    buffer_ = new char[buffer_size];
    for (auto &server_add : servers_add)
    {
        int key = hashKey(server_add.ip + std::to_string(server_add.port));
        hash_ring_.insert(key);
        server_map_[key] = server_add;
    }
}

LoadBalancer::~LoadBalancer()
{
    delete[] buffer_;
}

// Hash function
int LoadBalancer::hashKey(const std::string &key)
{
    std::hash<std::string> hash_fn;
    return static_cast<int>(hash_fn(key) & 0x7FFFFFFF);; // Normalize hash space
}

// Get the nearest server from the hash ring
ServerAdd LoadBalancer::getServer(const std::string &key)
{
    int hash = hashKey(key);
    auto it = hash_ring_.lower_bound(hash);
    if (it == hash_ring_.end())
        it = hash_ring_.begin();
    return server_map_[*it];
}

void LoadBalancer::server_init(const parsingKeyFuncPtr parse_key)
{
    struct sockaddr_in address;
    int lb_sockfd, epoll_fd;
    struct epoll_event event, events[max_events_];
    int addrlen = sizeof(address);

    lb_sockfd = create_non_locking_socket(lb_ip_, lb_port_, address);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("[LB]: Epoll creation failed");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = lb_sockfd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, lb_sockfd, &event) == -1)
    {
        perror("[LB]: Epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[LB]: Load Balancer listening on port " << lb_port_ << std::endl;

    while (true)
    {
        int num_events = epoll_wait(epoll_fd, events, max_events_, -1);
        if (num_events == -1)
        {
            perror("[LB]: Epoll wait failed");
            break;
        }

        for (int i = 0; i < num_events; ++i)
        {
            int sock_fd = events[i].data.fd;

            if (sock_fd == lb_sockfd)
            {
                // New client connection
                int client_fd = accept(lb_sockfd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
                int client_port = ntohs(address.sin_port);

                if (client_fd == -1)
                {
                    perror("[LB]: Accept failed");
                    continue;
                }

                set_nonblocking(client_fd);

                event.events = EPOLLIN | EPOLLET;
                event.data.fd = client_fd;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1)
                {
                    perror("Epoll_ctl client add failed");
                    close(client_fd);
                    continue;
                }

            //    std::cout << "[LB]: New client connected: " << client_ip << ":" << client_port << std::endl;
            }
            else
            {
                // Handle client request
                memset(buffer_, 0, buffer_size_);
                int bytes_read = recv(sock_fd, buffer_, buffer_size_, 0);

                if (bytes_read > 0)
                {
                    buffer_[bytes_read] = '\0';
                    std::string key = parse_key(buffer_, bytes_read);
                    ServerAdd server_add = getServer(key);

                    // Create a new connection to the backend server
                    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
                    struct sockaddr_in server_addr;
                    server_addr.sin_family = AF_INET;
                    server_addr.sin_port = htons(server_add.port);

                    if (inet_pton(AF_INET, server_add.ip.c_str(), &server_addr.sin_addr) <= 0)
                    {
                        perror("[LB]: Invalid IP address");
                        exit(EXIT_FAILURE);
                    }

                    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
                    {
                        send(server_fd, buffer_, bytes_read, 0);
                        // char response[buffer_size_] = {0};
                        int resp_bytes = recv(server_fd, buffer_, buffer_size_, 0);
                        if (resp_bytes > 0)
                        {
                            send(sock_fd, buffer_, resp_bytes, 0);
                        }
                        close(server_fd); // Close backend server connection after response
                    }
                    else
                    {
                        perror("[LB]:erver connection failed");
                        close(server_fd);
                    }
                }
                else
                {
                    // Client disconnected
                    // std::cout << "[LB]: Client " << sock_fd << " disconnected." << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, nullptr);
                    close(sock_fd);
                }
            }
        }
    }

    close(lb_sockfd);
    close(epoll_fd);
}

#endif