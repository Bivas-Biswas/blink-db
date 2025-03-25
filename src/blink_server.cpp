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

const int MAX_EVENTS = 4096;
const int BUFFER_SIZE = 2048;
const int SERVER_PORT = 9001;
const int MAX_MEMORY_BYTES = 1024 * 1024 * 1024;
const std::string SERVER_IP = "127.0.0.1";

int main() {
    Server server(SERVER_IP, SERVER_PORT, BUFFER_SIZE, MAX_EVENTS, MAX_MEMORY_BYTES);
    server.init();
    return 0;
}