#include <iostream>
#include <string>
#include "../lib/client.h"

/// Maximum number of events for epoll
const int MAX_EVENTS = 4096;
/// Buffer size for reading data
const int BUFFER_SIZE = 2048;
/// Server port to connect to
const int SERVER_PORT = 9001;
/// Server IP address
const std::string SERVER_IP = "127.0.0.1";

/**
 * @brief Main function to run the client application.
 * 
 * The client connects to the server and allows users to enter commands
 * such as SET, GET, DEL, and EXIT to interact with the key-value store.
 * 
 * @return int Returns 0 on success, -1 on failure.
 */
int main()
{
    Client client(SERVER_IP, SERVER_PORT, BUFFER_SIZE);

    /// Initialize server connection
    if (client.server_init() == -1)
    {
        return -1;
    }

    std::cout << "Server is connected at " << client.ip_addr << ":" << client.port << std::endl;
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

    /// Close the server connection
    client.close_server();

    return 0;
}