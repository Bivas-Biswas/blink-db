#include <iostream>
#include <string>
#include "../lib/server.h"

/// Maximum number of events for epoll
const int MAX_EVENTS = 4096;
/// Buffer size for reading data
const int BUFFER_SIZE = 2048;
/// Server port to bind to
const int SERVER_PORT = 9001;
/// Maximum memory allocation in bytes
const int MAX_MEMORY_BYTES = 1024 * 1024 * 1024;
/// Server IP address
const std::string SERVER_IP = "127.0.0.1";

/**
 * @brief Main function to start the server.
 * 
 * The server initializes and starts listening for incoming client connections.
 * 
 * @return int Returns 0 on successful execution.
 */
int main() {
    /// Create a Server instance with configuration parameters
    Server server(SERVER_IP, SERVER_PORT, BUFFER_SIZE, MAX_EVENTS, MAX_MEMORY_BYTES);
    
    /// Initialize the server
    server.init();
    
    return 0;
}