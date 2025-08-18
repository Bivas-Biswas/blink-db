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
#include <algorithm>  // For std::transform
#include <cctype>     // For tolower
#include <list>       // For implementing LRU
#include <chrono>     // For timestamp

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define PORT 9001
#define MAX_MEMORY_BYTES 1024 * 1024 * 10 // 100 MB memory limit

// LRU cache implementation
class LRUCache {
private:
    // Structure to hold key, value and metadata
    struct CacheEntry {
        std::string key;
        std::string value;
        std::chrono::steady_clock::time_point last_accessed;
        
        CacheEntry(const std::string& k, const std::string& v) 
            : key(k), value(v), last_accessed(std::chrono::steady_clock::now()) {}
    };
    
    // List to maintain LRU order (most recently used at front)
    std::list<CacheEntry> cache_list;
    
    // Map for O(1) lookups
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> cache_map;
    
    // Current memory usage
    size_t current_memory_usage;
    
    // Maximum memory limit
    size_t max_memory_bytes;
    
public:
    LRUCache(size_t max_mem) : current_memory_usage(0), max_memory_bytes(max_mem) {}
    
    // Set a key-value pair
    void set(const std::string& key, const std::string& value) {
        // Calculate memory for this entry (key + value + overhead)
        size_t entry_size = key.size() + value.size() + sizeof(CacheEntry);
        
        // Check if key already exists
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            // Update existing entry
            current_memory_usage -= it->second->key.size() + it->second->value.size();
            cache_list.erase(it->second);
            cache_map.erase(it);
        }
        
        // Check if we need to evict entries to make space
        while (current_memory_usage + entry_size > max_memory_bytes && !cache_list.empty()) {
            // Evict least recently used item
            auto last = cache_list.back();
            std::cout << "LRU Eviction: Removing key '" << last.key << "'" << std::endl;
            current_memory_usage -= last.key.size() + last.value.size();
            cache_map.erase(last.key);
            cache_list.pop_back();
        }
        
        // If we still can't fit the new entry, don't add it
        if (current_memory_usage + entry_size > max_memory_bytes) {
            std::cerr << "Warning: Entry too large to fit in cache" << std::endl;
            return;
        }
        
        // Add new entry to front of list (most recently used)
        cache_list.emplace_front(key, value);
        cache_map[key] = cache_list.begin();
        current_memory_usage += entry_size;
        
        std::cout << "Memory usage: " << current_memory_usage << "/" << max_memory_bytes 
                  << " bytes (" << (current_memory_usage * 100.0 / max_memory_bytes) << "%)" << std::endl;
    }
    
    // Get a value by key
    bool get(const std::string& key, std::string& value) {
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return false;  // Key not found
        }
        
        // Update access time and move to front of list
        it->second->last_accessed = std::chrono::steady_clock::now();
        
        // Move to front (most recently used)
        if (it->second != cache_list.begin()) {
            cache_list.splice(cache_list.begin(), cache_list, it->second);
        }
        
        value = it->second->value;
        return true;
    }
    
    // Delete a key
    bool del(const std::string& key) {
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return false;  // Key not found
        }
        
        // Update memory usage
        current_memory_usage -= it->second->key.size() + it->second->value.size();
        
        // Remove from list and map
        cache_list.erase(it->second);
        cache_map.erase(it);
        
        return true;
    }
    
    // Get current memory usage
    size_t memory_usage() const {
        return current_memory_usage;
    }
    
    // Get max memory limit
    size_t max_memory() const {
        return max_memory_bytes;
    }
    
    // Get number of items in cache
    size_t size() const {
        return cache_map.size();
    }
};

// Replace the simple database with our LRU cache
LRUCache database(MAX_MEMORY_BYTES);

// Function prototypes
void set_nonblocking(int sock);
std::vector<std::string> parse_resp(const std::string& input);
std::string encode_resp(const std::string& response, bool is_error);
std::string handle_command(const std::vector<std::string>& command);

int main() {
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct epoll_event event, events[MAX_EVENTS];
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    // Make server socket non-blocking
    set_nonblocking(server_fd);
    
    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Epoll creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Add server socket to epoll
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("Epoll_ctl failed");
        exit(EXIT_FAILURE);
    }
    
    std::cout << "Redis-compatible server listening on port " << PORT << std::endl;
    std::cout << "Memory limit set to " << (MAX_MEMORY_BYTES / (1024 * 1024)) << " MB with LRU eviction policy" << std::endl;
    
    // Main event loop
    while (true) {
        int ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (ready_fds == -1) {
            perror("Epoll wait failed");
            break;
        }
        
        for (int i = 0; i < ready_fds; i++) {
            if (events[i].data.fd == server_fd) {
                // New connection
                int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                if (client_fd == -1) {
                    perror("Accept failed");
                    continue;
                }
                
                // Set client socket to non-blocking
                set_nonblocking(client_fd);
                
                // Add client to epoll
                event.events = EPOLLIN | EPOLLET;  // Edge-triggered
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("Epoll_ctl client add failed");
                    close(client_fd);
                    continue;
                }
                
                std::cout << "New client connected: " << client_fd << std::endl;
            } else {
                // Client data available
                char buffer[BUFFER_SIZE];
                int client_fd = events[i].data.fd;
                
                int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
                if (bytes_read <= 0) {
                    // Connection closed or error
                    std::cout << "Client disconnected: " << client_fd << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    continue;
                }
                
                buffer[bytes_read] = '\0';
                std::string input(buffer, bytes_read);
                
                // Parse RESP protocol
                std::vector<std::string> command = parse_resp(input);
                
                // Handle command
                std::string response = handle_command(command);
                
                // Send response
                send(client_fd, response.c_str(), response.length(), 0);
            }
        }
    }
    
    close(server_fd);
    close(epoll_fd);
    return 0;
}

// Set socket to non-blocking mode
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        exit(EXIT_FAILURE);
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL failed");
        exit(EXIT_FAILURE);
    }
}

// Parse RESP protocol input
std::vector<std::string> parse_resp(const std::string& input) {
    std::vector<std::string> result;
    // std::cout << input << std::endl;
    
    if (input.empty()) {
        return result;
    }
    
    // Check if it's an array
    if (input[0] == '*') {
        size_t pos = 1;
        size_t newline = input.find("\r\n", pos);
        int array_len = std::stoi(input.substr(pos, newline - pos));
        
        pos = newline + 2;  // Skip \r\n
        
        for (int i = 0; i < array_len; i++) {
            if (pos >= input.length()) break;
            
            // Bulk string
            if (input[pos] == '$') {
                pos++;  // Skip $
                newline = input.find("\r\n", pos);
                int str_len = std::stoi(input.substr(pos, newline - pos));
                
                pos = newline + 2;  // Skip \r\n
                result.push_back(input.substr(pos, str_len));
                pos += str_len + 2;  // Skip string and \r\n
            }
        }
    }
    
    return result;
}

// Encode response in RESP format
std::string encode_resp(const std::string& response, bool is_error) {
    if (is_error) {
        return "-ERR " + response + "\r\n";
    } else if (response.empty()) {
        return "$-1\r\n";  // Null bulk string
    } else {
        return "+" + response + "\r\n";  // Simple string
    }
}

// Handle Redis commands
std::string handle_command(const std::vector<std::string>& command) {
    if (command.empty()) {
        return encode_resp("Invalid command", true);
    }
    
    std::string cmd = command[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    if (cmd == "SET") {
        if (command.size() < 3) {
            return encode_resp("SET command requires key and value", true);
        }
        
        database.set(command[1], command[2]);
        return encode_resp("OK", false);
    } 
    else if (cmd == "GET") {
        if (command.size() < 2) {
            return encode_resp("GET command requires key", true);
        }
        
        std::string value;
        if (database.get(command[1], value)) {
            return "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
        } else {
            return "$-1\r\n";  // Null bulk string for non-existent key
        }
    } 
    else if (cmd == "DEL") {
        if (command.size() < 2) {
            return encode_resp("DEL command requires key", true);
        }
        
        int count = 0;
        for (size_t i = 1; i < command.size(); i++) {
            count += database.del(command[i]) ? 1 : 0;
        }
        
        return ":" + std::to_string(count) + "\r\n";  // Integer response
    }
    else if (cmd == "INFO") {
        // Add INFO command to get memory usage statistics
        std::string info = "# Memory\r\n";
        info += "used_memory:" + std::to_string(database.memory_usage()) + "\r\n";
        info += "maxmemory:" + std::to_string(database.max_memory()) + "\r\n";
        info += "maxmemory_policy:allkeys-lru\r\n";
        info += "# Stats\r\n";
        info += "keyspace_hits:" + std::to_string(database.size()) + "\r\n";
        
        return "$" + std::to_string(info.length()) + "\r\n" + info + "\r\n";
    }
    else if (cmd == "CONFIG") {
        // Basic CONFIG command implementation
        if (command.size() < 2) {
            return encode_resp("CONFIG command requires subcommand", true);
        }
        
        std::string subcmd = command[1];
        std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::toupper);
        
        if (subcmd == "GET" && command.size() >= 3) {
            std::string param = command[2];
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);
            
            if (param == "maxmemory") {
                return "*2\r\n$9\r\nmaxmemory\r\n$" + std::to_string(std::to_string(database.max_memory()).length()) + 
                       "\r\n" + std::to_string(database.max_memory()) + "\r\n";
            }
            else if (param == "maxmemory-policy") {
                return "*2\r\n$16\r\nmaxmemory-policy\r\n$11\r\nallkeys-lru\r\n";
            }
        }
        
        return encode_resp("Supported CONFIG commands: GET maxmemory, GET maxmemory-policy", false);
    }
    else {
        return encode_resp("Unknown command", true);
    }
}
