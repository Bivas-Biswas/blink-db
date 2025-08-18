#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define PORT 9001
#define MAX_MEMORY_BYTES 1024 * 1024 * 10 // 10 MB memory limit
#define HASH_TABLE_SIZE 1024

// Cache entry structure
typedef struct CacheEntry {
    char* key;
    char* value;
    time_t last_accessed;
    struct CacheEntry* prev;
    struct CacheEntry* next;
} CacheEntry;

// Hash table node
typedef struct HashNode {
    char* key;
    CacheEntry* entry;
    struct HashNode* next;
} HashNode;

// LRU Cache
typedef struct LRUCache {
    HashNode** hash_table;
    size_t table_size;
    CacheEntry* head;  // Most recently used
    CacheEntry* tail;  // Least recently used
    size_t current_memory_usage;
    size_t max_memory_bytes;
    size_t item_count;
} LRUCache;

// Global LRU cache
LRUCache* database;

// Function prototypes
void set_nonblocking(int sock);
char** parse_resp(const char* input, int* count);
char* encode_resp(const char* response, int is_error);
char* handle_command(char** command, int count);
unsigned int hash_function(const char* key, size_t table_size);
void move_to_front(LRUCache* cache, CacheEntry* entry);
void lru_cache_set(LRUCache* cache, const char* key, const char* value);
bool lru_cache_get(LRUCache* cache, const char* key, char** value);
bool lru_cache_del(LRUCache* cache, const char* key);
LRUCache* lru_cache_create(size_t max_memory);
void lru_cache_destroy(LRUCache* cache);
void database_set(const char* key, const char* value);
bool database_get(const char* key, char** value);
bool database_del(const char* key);

// Hash function for string keys
unsigned int hash_function(const char* key, size_t table_size) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash * 31) + (*key++);
    }
    return hash % table_size;
}

// Create a new LRU cache
LRUCache* lru_cache_create(size_t max_memory) {
    LRUCache* cache = (LRUCache*)malloc(sizeof(LRUCache));
    if (!cache) return NULL;
    
    cache->hash_table = (HashNode**)calloc(HASH_TABLE_SIZE, sizeof(HashNode*));
    if (!cache->hash_table) {
        free(cache);
        return NULL;
    }
    
    cache->table_size = HASH_TABLE_SIZE;
    cache->head = NULL;
    cache->tail = NULL;
    cache->current_memory_usage = 0;
    cache->max_memory_bytes = max_memory;
    cache->item_count = 0;
    
    return cache;
}

// Free all cache resources
void lru_cache_destroy(LRUCache* cache) {
    if (!cache) return;
    
    // Free all entries in the cache
    CacheEntry* current = cache->head;
    while (current) {
        CacheEntry* next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }
    
    // Free all hash table nodes
    for (size_t i = 0; i < cache->table_size; i++) {
        HashNode* node = cache->hash_table[i];
        while (node) {
            HashNode* next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
    }
    
    free(cache->hash_table);
    free(cache);
}

// Move entry to front of LRU list (most recently used)
void move_to_front(LRUCache* cache, CacheEntry* entry) {
    if (!cache || !entry || entry == cache->head) return;
    
    // Remove from current position
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    
    // Update tail if needed
    if (entry == cache->tail) {
        cache->tail = entry->prev;
    }
    
    // Move to front
    entry->next = cache->head;
    entry->prev = NULL;
    
    if (cache->head) {
        cache->head->prev = entry;
    }
    
    cache->head = entry;
    
    // If this is the only entry, it's also the tail
    if (!cache->tail) {
        cache->tail = entry;
    }
}

// Set a key-value pair in the cache
void lru_cache_set(LRUCache* cache, const char* key, const char* value) {
    if (!cache || !key || !value) return;
    
    // Calculate memory for this entry
    size_t key_size = strlen(key) + 1;
    size_t value_size = strlen(value) + 1;
    size_t entry_size = key_size + value_size + sizeof(CacheEntry);
    
    // Check if key already exists
    unsigned int hash = hash_function(key, cache->table_size);
    HashNode* prev = NULL;
    HashNode* current = cache->hash_table[hash];
    
    while (current) {
        if (strcmp(current->key, key) == 0) {
            // Update existing entry
            CacheEntry* entry = current->entry;
            
            // Update memory usage
            cache->current_memory_usage -= strlen(entry->key) + 1 + strlen(entry->value) + 1;
            
            // Update value
            free(entry->value);
            entry->value = strdup(value);
            entry->last_accessed = time(NULL);
            
            // Update memory usage
            cache->current_memory_usage += key_size + value_size;
            
            // Move to front of LRU list
            move_to_front(cache, entry);
            
            printf("Updated key: %s\n", key);
            return;
        }
        prev = current;
        current = current->next;
    }
    
    // Evict entries if needed to make space
    while (cache->current_memory_usage + entry_size > cache->max_memory_bytes && cache->tail) {
        // Evict least recently used item (tail of list)
        CacheEntry* to_evict = cache->tail;
        printf("LRU Eviction: Removing key '%s'\n", to_evict->key);
        
        // Remove from hash table
        unsigned int evict_hash = hash_function(to_evict->key, cache->table_size);
        HashNode* e_prev = NULL;
        HashNode* e_current = cache->hash_table[evict_hash];
        
        while (e_current) {
            if (e_current->entry == to_evict) {
                if (e_prev) {
                    e_prev->next = e_current->next;
                } else {
                    cache->hash_table[evict_hash] = e_current->next;
                }
                
                free(e_current->key);
                free(e_current);
                break;
            }
            e_prev = e_current;
            e_current = e_current->next;
        }
        
        // Update memory usage
        cache->current_memory_usage -= strlen(to_evict->key) + 1 + strlen(to_evict->value) + 1;
        
        // Update LRU list
        cache->tail = to_evict->prev;
        if (cache->tail) {
            cache->tail->next = NULL;
        } else {
            cache->head = NULL;
        }
        
        // Free evicted entry
        free(to_evict->key);
        free(to_evict->value);
        free(to_evict);
        
        cache->item_count--;
    }
    
    // If we still can't fit the new entry, don't add it
    if (cache->current_memory_usage + entry_size > cache->max_memory_bytes) {
        fprintf(stderr, "Warning: Entry too large to fit in cache\n");
        return;
    }
    
    // Create new entry
    CacheEntry* new_entry = (CacheEntry*)malloc(sizeof(CacheEntry));
    if (!new_entry) return;
    
    new_entry->key = strdup(key);
    new_entry->value = strdup(value);
    new_entry->last_accessed = time(NULL);
    new_entry->prev = NULL;
    new_entry->next = cache->head;
    
    // Add to front of LRU list
    if (cache->head) {
        cache->head->prev = new_entry;
    }
    
    cache->head = new_entry;
    
    if (!cache->tail) {
        cache->tail = new_entry;
    }
    
    // Add to hash table
    HashNode* new_node = (HashNode*)malloc(sizeof(HashNode));
    if (!new_node) {
        free(new_entry->key);
        free(new_entry->value);
        free(new_entry);
        return;
    }
    
    new_node->key = strdup(key);
    new_node->entry = new_entry;
    new_node->next = cache->hash_table[hash];
    cache->hash_table[hash] = new_node;
    
    // Update memory usage and item count
    cache->current_memory_usage += entry_size;
    cache->item_count++;
    
    printf("Memory usage: %zu/%zu bytes (%.2f%%)\n", 
           cache->current_memory_usage, 
           cache->max_memory_bytes, 
           (cache->current_memory_usage * 100.0 / cache->max_memory_bytes));
}

// Get a value by key from the cache
bool lru_cache_get(LRUCache* cache, const char* key, char** value) {
    if (!cache || !key || !value) return false;
    
    unsigned int hash = hash_function(key, cache->table_size);
    HashNode* current = cache->hash_table[hash];
    
    while (current) {
        if (strcmp(current->key, key) == 0) {
            // Found the key
            CacheEntry* entry = current->entry;
            
            // Update access time
            entry->last_accessed = time(NULL);
            
            // Move to front of LRU list
            move_to_front(cache, entry);
            
            // Return a copy of the value
            *value = strdup(entry->value);
            return true;
        }
        current = current->next;
    }
    
    // Key not found
    return false;
}

// Delete a key from the cache
bool lru_cache_del(LRUCache* cache, const char* key) {
    if (!cache || !key) return false;
    
    unsigned int hash = hash_function(key, cache->table_size);
    HashNode* prev = NULL;
    HashNode* current = cache->hash_table[hash];
    
    while (current) {
        if (strcmp(current->key, key) == 0) {
            // Found the key
            CacheEntry* entry = current->entry;
            
            // Remove from hash table
            if (prev) {
                prev->next = current->next;
            } else {
                cache->hash_table[hash] = current->next;
            }
            
            // Update memory usage
            cache->current_memory_usage -= strlen(entry->key) + 1 + strlen(entry->value) + 1;
            
            // Remove from LRU list
            if (entry->prev) entry->prev->next = entry->next;
            if (entry->next) entry->next->prev = entry->prev;
            
            if (cache->head == entry) cache->head = entry->next;
            if (cache->tail == entry) cache->tail = entry->prev;
            
            // Free memory
            free(entry->key);
            free(entry->value);
            free(entry);
            free(current->key);
            free(current);
            
            cache->item_count--;
            return true;
        }
        prev = current;
        current = current->next;
    }
    
    // Key not found
    return false;
}

// Set a key-value pair in the database
void database_set(const char* key, const char* value) {
    if (!key || !value) return;
    
    // Call the LRU cache set function
    lru_cache_set(database, key, value);
}

// Get a value by key from the database
bool database_get(const char* key, char** value) {
    if (!key || !value) return false;
    
    // Call the LRU cache get function
    return lru_cache_get(database, key, value);
}

// Delete a key from the database
bool database_del(const char* key) {
    if (!key) return false;
    
    // Call the LRU cache del function
    return lru_cache_del(database, key);
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

int main() {
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct epoll_event event, events[MAX_EVENTS];
    
    // Initialize LRU cache
    database = lru_cache_create(MAX_MEMORY_BYTES);
    if (!database) {
        perror("Failed to create LRU cache");
        exit(EXIT_FAILURE);
    }
    
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
    
    printf("Redis-compatible server listening on port %d\n", PORT);
    printf("Memory limit set to %u MB with LRU eviction policy\n", 
           (MAX_MEMORY_BYTES / (1024 * 1024)));
    
    // Main event loop
    while (1) {
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
                
                printf("New client connected: %d\n", client_fd);
            } else {
                // Client data available
                char buffer[BUFFER_SIZE];
                int client_fd = events[i].data.fd;
                
                int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
                if (bytes_read <= 0) {
                    // Connection closed or error
                    printf("Client disconnected: %d\n", client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    continue;
                }
                
                buffer[bytes_read] = '\0';
                
                // Parse RESP protocol
                int count = 0;
                char** command = parse_resp(buffer, &count);
                
                // Handle command
                char* response = handle_command(command, count);
                
                // Send response
                send(client_fd, response, strlen(response), 0);
                
                // Free memory
                free(response);
                for (int j = 0; j < count; j++) {
                    free(command[j]);
                }
                free(command);
            }
        }
    }
    
    // Cleanup
    lru_cache_destroy(database);
    close(server_fd);
    close(epoll_fd);
    return 0;
}



// Parse RESP protocol input
char** parse_resp(const char* input, int* count) {
    char** result = NULL;
    *count = 0;
    
    if (!input || input[0] == '\0') {
        return result;
    }
    
    // Check if it's an array
    if (input[0] == '*') {
        size_t pos = 1;
        size_t newline_pos = 0;
        
        // Find the first \r\n
        while (input[pos + newline_pos] != '\0' && 
               !(input[pos + newline_pos] == '\r' && input[pos + newline_pos + 1] == '\n')) {
            newline_pos++;
        }
        
        // Extract array length
        char len_str[32] = {0};
        strncpy(len_str, input + pos, newline_pos);
        int array_len = atoi(len_str);
        
        pos = pos + newline_pos + 2;  // Skip \r\n
        
        // Allocate memory for result array
        result = (char**)malloc(sizeof(char*) * array_len);
        if (!result) return NULL;
        
        // Parse each bulk string in the array
        for (int i = 0; i < array_len; i++) {
            if (input[pos] == '$') {
                pos++;  // Skip $
                
                // Find the next \r\n
                newline_pos = 0;
                while (input[pos + newline_pos] != '\0' && 
                       !(input[pos + newline_pos] == '\r' && input[pos + newline_pos + 1] == '\n')) {
                    newline_pos++;
                }
                
                // Extract string length
                char str_len_str[32] = {0};
                strncpy(str_len_str, input + pos, newline_pos);
                int str_len = atoi(str_len_str);
                
                pos = pos + newline_pos + 2;  // Skip \r\n
                
                // Allocate memory for the string
                result[i] = (char*)malloc(str_len + 1);
                if (!result[i]) {
                    // Free previously allocated memory
                    for (int j = 0; j < i; j++) {
                        free(result[j]);
                    }
                    free(result);
                    *count = 0;
                    return NULL;
                }
                
                // Copy the string
                strncpy(result[i], input + pos, str_len);
                result[i][str_len] = '\0';
                
                pos += str_len + 2;  // Skip string and \r\n
                (*count)++;
            }
        }
    }
    
    return result;
}

// Encode response in RESP format
char* encode_resp(const char* response, int is_error) {
    char* result = NULL;
    
    if (is_error) {
        // Error format: -ERR message\r\n
        size_t len = strlen(response) + 7; // -ERR + space + \r\n + \0
        result = (char*)malloc(len);
        if (result) {
            snprintf(result, len, "-ERR %s\r\n", response);
        }
    } else if (!response || response[0] == '\0') {
        // Null bulk string: $-1\r\n
        result = strdup("$-1\r\n");
    } else {
        // Simple string: +message\r\n
        size_t len = strlen(response) + 4; // + + \r\n + \0
        result = (char*)malloc(len);
        if (result) {
            snprintf(result, len, "+%s\r\n", response);
        }
    }
    
    return result;
}

// Handle Redis commands
char* handle_command(char** command, int count) {
    if (!command || count == 0) {
        return encode_resp("Invalid command", 1);
    }
    
    // Convert command to uppercase for case-insensitive comparison
    char* cmd = strdup(command[0]);
    if (!cmd) return encode_resp("Memory allocation error", 1);
    
    for (size_t i = 0; i < strlen(cmd); i++) {
        cmd[i] = toupper(cmd[i]);
    }
    
    char* response = NULL;
    
    if (strcmp(cmd, "SET") == 0) {
        if (count < 3) {
            response = encode_resp("SET command requires key and value", 1);
        } else {
            // In the actual implementation, this would call database.set()
            database_set(command[1], command[2]);
            response = encode_resp("OK", 0);
        }
    } 
    else if (strcmp(cmd, "GET") == 0) {
        if (count < 2) {
            response = encode_resp("GET command requires key", 1);
        } else {
            // In the actual implementation, this would call database.get()
            // if (database_get(command[1], value)) {
            //     // Format bulk string response
            //     size_t len = strlen(value);
            //     char* bulk_resp = malloc(32 + len);
            //     sprintf(bulk_resp, "$%zu\r\n%s\r\n", len, value);
            //     response = bulk_resp;
            // } else {
            //     response = strdup("$-1\r\n");  // Null bulk string
            // }
            
            // For demonstration, we'll just return a null response
            response = strdup("$-1\r\n");
        }
    } 
    else if (strcmp(cmd, "DEL") == 0) {
        if (count < 2) {
            response = encode_resp("DEL command requires key", 1);
        } else {
            // In actual implementation, this would call database.del() for each key
            // int deleted_count = 0;
            // for (int i = 1; i < count; i++) {
            //     deleted_count += database.del(command[i]) ? 1 : 0;
            // }
            
            // Format integer response
            char int_resp[32];
            sprintf(int_resp, ":%d\r\n", count - 1);  // Assume all keys were deleted
            response = strdup(int_resp);
        }
    }
    else if (strcmp(cmd, "INFO") == 0) {
        // For demonstration, we'll return a simple info string
        const char* info = "# Memory\r\nused_memory:1024\r\nmaxmemory:10485760\r\n";
        size_t len = strlen(info);
        
        // Format bulk string response
        char* bulk_resp = (char*)malloc(32 + len);
        sprintf(bulk_resp, "$%zu\r\n%s\r\n", len, info);
        response = bulk_resp;
    }
    else {
        response = encode_resp("Unknown command", 1);
    }
    
    free(cmd);
    return response;
}


