#ifndef SERVER_BLINK_H
#define SERVER_BLINK_H

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <list>
#include <chrono>

#include "./lru_cache.h"
#include "../utils/create_non_locking_socket.h"
#include "../utils/set_nonblocking.h"

/**
 * @class Server
 * @brief Implements a Blink-compatible server with an LRU-based in-memory database.
 */
class Server
{
private:
    std::string ip_;         ///< Server IP address.
    int port_;               ///< Server port number.
    int max_events_ = 10;    ///< Maximum epoll events.
    int buffer_size_ = 1024; ///< Buffer size for receiving data.
    char *buffer_;           ///< Buffer for storing client requests.
    int max_mem_bytes_;      ///< Maximum memory allocated for caching.
    LRUCache database_;      ///< LRU cache-based database.
    std::string tag_;

    /**
     * @brief Parses a RESP (Redis Serialization Protocol) formatted string.
     * @param input Input string in RESP format.
     * @param result Vector to store parsed tokens.
     */
    void parse_resp(const std::string &input, std::vector<std::string> &result);

    /**
     * @brief Encodes a response string into RESP format.
     * @param response Response string to encode.
     * @param is_error Whether the response is an error message.
     */
    void encode_resp(std::string &response, bool is_error);

    /**
     * @brief Handles client commands and generates appropriate responses.
     * @param command Parsed command tokens.
     * @param response String to store the response.
     */
    void handle_command(const std::vector<std::string> &command, std::string &response);

public:
    /**
     * @brief Constructs a Server object.
     * @param ip Server IP address.
     * @param port Server port number.
     * @param buffer_size Buffer size for receiving data.
     * @param max_events Maximum epoll events.
     * @param max_mem_bytes Maximum memory allocation for caching.
     */
    Server(std::string ip, int port, int buffer_size, int max_events, int max_mem_bytes);

    /**
     * @brief Destructor to release allocated resources.
     */
    ~Server();

    /**
     * @brief Initializes the server, sets up epoll, and starts listening for connections.
     */
    void init();
};

Server::Server(std::string ip, int port, int buffer_size, int max_events, int max_mem_bytes)
    : ip_(ip),
      database_(max_mem_bytes)
{
    port_ = port;
    buffer_size_ = buffer_size;
    max_mem_bytes_ = max_mem_bytes;
    max_events_ = max_events;
    tag_ = "[" + ip + ":" + std::to_string(port) + "] ";
    buffer_ = new char[buffer_size];
}

Server::~Server()
{
    delete[] buffer_;
}

void Server::init()
{
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    struct epoll_event event, events[max_events_];

    server_fd = create_non_locking_socket(ip_, port_, address);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("[Server]: Epoll creation failed");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
    {
        perror("[Server] Epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    std::cout << tag_ << "Blink-compatible server listening on port " << port_ << std::endl;
    std::cout << tag_ << "Memory limit set to " << (max_mem_bytes_ / (1024 * 1024)) << " MB with LRU eviction policy" << std::endl;

    std::vector<std::string> result;
    std::string response;

    while (true)
    {
        int ready_fds = epoll_wait(epoll_fd, events, max_events_, -1);
        if (ready_fds == -1)
        {
            perror("Epoll wait failed");
            break;
        }

        for (int i = 0; i < ready_fds; i++)
        {
            int sock_fd = events[i].data.fd;

            if (sock_fd == server_fd)
            {
                int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);

                // char client_ip[INET_ADDRSTRLEN];
                // inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
                // int client_port = ntohs(address.sin_port);

                if (client_fd == -1)
                {
                    perror("Accept failed");
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

                // std::cout << tag_ << "New client connected: " << client_ip << ":" << client_port << std::endl;
            }
            else
            {
                memset(buffer_, 0, buffer_size_);
                int bytes_read = recv(sock_fd, buffer_, buffer_size_, 0);

                if (bytes_read > 0)
                {
                    buffer_[bytes_read] = '\0';
                    std::string input(buffer_, bytes_read);

                    parse_resp(input, result);
                    handle_command(result, response);

                    send(sock_fd, response.c_str(), response.length(), 0);
                }
                else
                {
                    // std::cout << tag_ << "Client " << sock_fd << " disconnected." << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, nullptr);
                    close(sock_fd);
                }

                result.clear();
                response.clear();
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
}

void Server::parse_resp(const std::string &input, std::vector<std::string> &result)
{
    if (input.empty())
        return;

    if (input[0] == '*')
    {
        size_t pos = 1;
        size_t newline = input.find("\r\n", pos);
        int array_len = std::stoi(input.substr(pos, newline - pos));

        pos = newline + 2;
        for (int i = 0; i < array_len; i++)
        {
            if (pos >= input.length())
                break;

            if (input[pos] == '$')
            {
                pos++;
                newline = input.find("\r\n", pos);
                int str_len = std::stoi(input.substr(pos, newline - pos));

                pos = newline + 2;
                result.push_back(input.substr(pos, str_len));
                pos += str_len + 2;
            }
        }
    }
}

void Server::encode_resp(std::string &response, bool is_error)
{
    if (is_error)
    {
        response = "-ERR " + response + "\r\n";
    }
    else if (response.empty())
    {
        response = "$-1\r\n";
    }
    else
    {
        response = "+" + response + "\r\n";
    }
}

void Server::handle_command(const std::vector<std::string> &command, std::string &response)
{
    if (command.empty())
    {
        response = "Invalid command";
        encode_resp(response, true);
        return;
    }

    std::string cmd = command[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "SET")
    {
        if (command.size() < 3)
        {
            response = "SET command requires key and value";
            encode_resp(response, true);
            return;
        }

        database_.set(command[1], command[2]);
        response = "OK";
        encode_resp(response, false);
    }
    else if (cmd == "GET")
    {
        if (command.size() < 2)
        {
            response = "GET command requires key";
            encode_resp(response, true);
            return;
        }

        std::string value;
        if (database_.get(command[1], value))
        {
            response = "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
        }
        else
        {
            response = "$-1\r\n";
        }
    }
    else if (cmd == "DEL")
    {
        if (command.size() < 2)
        {
            response = "DEL command requires key";
            encode_resp(response, true);
            return;
        }

        int count = 0;
        for (size_t i = 1; i < command.size(); i++)
        {
            count += database_.del(command[i]) ? 1 : 0;
        }

        response = ":" + std::to_string(count) + "\r\n";
    }

    else if (cmd == "INFO")
    {
        // Add INFO command to get memory usage statistics
        std::string info = "# Memory\r\n";
        info += "used_memory:" + std::to_string(database_.memory_usage()) + "\r\n";
        info += "maxmemory:" + std::to_string(database_.max_memory()) + "\r\n";
        info += "maxmemory_policy:allkeys-lru\r\n";
        info += "# Stats\r\n";
        info += "keyspace_hits:" + std::to_string(database_.size()) + "\r\n";

        response = "$" + std::to_string(info.length()) + "\r\n" + info + "\r\n";
    }
    else if (cmd == "CONFIG")
    {
        // Basic CONFIG command implementation
        if (command.size() < 2)
        {
            response = "CONFIG command requires subcommand";
            encode_resp(response, true);
            return;
        }

        std::string subcmd = command[1];
        std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::toupper);

        if (subcmd == "GET" && command.size() >= 3)
        {
            std::string param = command[2];
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);

            if (param == "maxmemory")
            {
                response = "*2\r\n$9\r\nmaxmemory\r\n$" + std::to_string(std::to_string(database_.max_memory()).length()) +
                           "\r\n" + std::to_string(database_.max_memory()) + "\r\n";
                return;
            }
            else if (param == "maxmemory-policy")
            {
                response = "*2\r\n$16\r\nmaxmemory-policy\r\n$11\r\nallkeys-lru\r\n";
                return;
            }
        }
        response = "Supported CONFIG commands: GET maxmemory, GET maxmemory-policy";
        encode_resp(response, false);
    }

    else
    {
        response = "Unknown command";
        encode_resp(response, true);
    }
}

#endif
