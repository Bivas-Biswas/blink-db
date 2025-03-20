#include <iostream>
#include "../lib/client.h"

#define BUFFER_SIZE 1024
#define SERVER_PORT 9001
#define SERVER_IP "127.0.0.1"

/**
 * @brief Reads user input, parses commands, and interacts with the Client class.
 * @param client A reference to the Client object.
 */
void command_loop(Client &client)
{
    std::cout << "Server is connected at " << client.ip_addr_ << ":" << client.port_ << std::endl;
    std::cout << "Enter command (SET key value, GET key, DEL key, or EXIT to quit):" << std::endl;

    std::string input, command, key, value;
    while (true)
    {
        std::cout << "> ";
        std::getline(std::cin, input);

        std::istringstream iss(input);
        iss >> command;
        if (command == "SET")
        {
            iss >> key >> value;
            if (key.empty() || value.empty())
            {
                std::cout << "Invalid SET command. Usage: SET <key> <value>" << std::endl;
                continue;
            }
            std::cout << client.set(key, value) << std::endl;
        }
        else if (command == "GET")
        {
            iss >> key;
            if (key.empty())
            {
                std::cout << "Invalid GET command. Usage: GET <key>" << std::endl;
                continue;
            }
            std::cout << client.get(key) << std::endl;
        }
        else if (command == "DEL")
        {
            iss >> key;
            if (key.empty())
            {
                std::cout << "Invalid DEL command. Usage: DEL <key>" << std::endl;
                continue;
            }
            std::cout << client.del(key) << std::endl;
        }
        else if (command == "EXIT")
        {
            break;
        }
        else
        {
            std::cout << "Unknown command. Use SET, GET, DEL, or EXIT." << std::endl;
        }
    }
}

int main()
{
    Client client(SERVER_IP, SERVER_PORT, BUFFER_SIZE);
    
    if (client.server_init() == -1)
    {
        return -1;
    }

    command_loop(client);

    client.close_server();

    return 0;
}