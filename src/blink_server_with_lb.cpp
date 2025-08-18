#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <thread>

#include "../lib/server.h"
#include "../lib/load_balancer.h"

#define MAX_EVENTS 100
#define BUFFER_SIZE 1024
#define SERVER_PORT 9001
#define MAX_MEMORY_BYTES 1024 * 1024 * 10 // 10 MB memory limit
#define SERVER_IP "127.0.0.1"

// Server function
void serverThread(std::string ip, int port) {
    Server server(ip, port, BUFFER_SIZE, MAX_EVENTS, MAX_MEMORY_BYTES);
    server.init();
}

std::string parse_key(char *buffer, int bytes_read){
    
    std::string input(buffer, bytes_read);
    std::vector<std::string> result;

    if (input.empty())
        return "";

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

    return result[1];
}

int main() {

    int num_servers;
    std::cout << "Enter number of servers: ";
    std::cin >> num_servers;

    std::vector<ServerAdd> servers_addr;
    for (int i = 0; i < num_servers; ++i) {
        ServerAdd server_add;
        server_add.port = 5000 + i;
        server_add.ip = SERVER_IP;
        servers_addr.push_back(server_add);
    }

    std::vector<std::thread> server_threads;
    for (auto &server_addr : servers_addr) {
        server_threads.emplace_back(serverThread, server_addr.ip, server_addr.port);
    }

    LoadBalancer loadBalancer(SERVER_IP, SERVER_PORT, servers_addr, BUFFER_SIZE, MAX_EVENTS);

    loadBalancer.server_init(&parse_key);

    for (auto& t : server_threads) t.join();

    return 0;
}