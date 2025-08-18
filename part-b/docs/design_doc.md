# Blink-Compatible Server Design Document

## Overview
The Blink-Compatible Server is an event-driven in-memory database server that supports an LRU cache for efficient data storage and retrieval. It uses the Redis Serialization Protocol (RESP) for communication and leverages epoll for handling multiple client connections asynchronously.

## Components

### 1. **Server Class**
#### **Attributes**
- `std::string ip` – Server IP address.
- `int port` – Server port number.
- `int max_events` – Maximum epoll events handled simultaneously.
- `int buffer_size` – Size of the receive buffer.
- `char *buffer` – Pointer to the allocated memory buffer for client communication.
- `int max_mem_bytes` – Maximum allocated memory for the in-memory cache.
- `LRUCache database` – LRU cache implementation for storing key-value pairs.
- `std::string tag` – Server identification tag for logs.

#### **Methods**
- `Server(std::string ip, int port, int buffer_size, int max_events, int max_mem_bytes)` – Constructor that initializes the server configuration.
- `~Server()` – Destructor to release allocated memory.
- `void init()` – Initializes the server, sets up epoll, and starts listening for connections.
- `void parse_resp(const std::string &input, std::vector<std::string> &result)` – Parses RESP-formatted client requests.
- `void encode_resp(std::string &response, bool is_error)` – Encodes responses in RESP format.
- `void handle_command(const std::vector<std::string> &command, std::string &response)` – Processes client commands and generates appropriate responses.

### 2. **Utilities**
- `set_nonblocking(int sock)` – Configures a socket to operate in non-blocking mode.
- `create_non_locking_socket(std::string ip, int port, sockaddr_in &address)` – Creates a non-locking socket for client connections.

### 3. **LRU Cache**
The server maintains an LRU (Least Recently Used) cache to store key-value pairs efficiently within the allocated memory limit.

## Architecture

### **1. Server Initialization**
- Creates a non-blocking socket bound to the specified IP and port.
- Initializes epoll and registers the server socket for incoming connections.
- Prints server startup details including memory limits and eviction policy.

### **2. Event Loop Handling (epoll-based)**
- Uses `epoll_wait()` to monitor events on active file descriptors.
- Accepts new client connections and sets them to non-blocking mode.
- Reads client requests, parses commands, and sends responses.
- Handles disconnections by removing clients from the epoll event list.

### **3. Command Handling**
The server supports the following Redis-like commands:

| Command  | Description |
|----------|-------------|
| `SET key value` | Stores a key-value pair in the LRU cache. |
| `GET key` | Retrieves the value associated with a key. |
| `DEL key` | Deletes a key from the cache. |
| `INFO` | Returns server statistics including memory usage and cache size. |
| `CONFIG GET maxmemory` | Returns the maximum allocated memory limit. |
| `CONFIG GET maxmemory-policy` | Returns the current memory eviction policy. |

## Error Handling
- Invalid commands return a `-ERR` response.
- If the server fails to initialize epoll or a socket operation, it logs an error and exits.
- Memory allocation failures result in server termination.

## Conclusion
This Blink-Compatible Server provides an efficient in-memory database solution with LRU eviction, non-blocking I/O, and an event-driven architecture. It serves as a lightweight alternative to Redis for specific applications requiring high-performance key-value storage.


# Blink Client Architecture and Implementation

## Overview
The `Client` class provides an interface to interact with a Blink server using the RESP (Blink Serialization Protocol). It supports key-value operations such as `SET`, `GET`, and `DEL` over a TCP socket connection.

## Components
### 1. **Client Class**
The `Client` class is responsible for managing the connection, sending commands, and processing responses.

#### **Data Members**
- `buffer_size`: Defines the size of the response buffer.
- `ip_addr`: Stores the IP address of the Blink server.
- `buffer`: A dynamically allocated buffer for receiving data.
- `port`: Stores the server's port number.
- `sock`: Socket descriptor for communication.
- `command`: A vector storing parsed command arguments.

#### **Methods**
##### **Constructor & Destructor**
- `Client(std::string _ip_addr, int _port, int _buffer_size)`: Initializes the client with server details and buffer size.
- `~Client()`: Frees allocated memory for the buffer.

##### **Server Connection Management**
- `int server_init()`: Initializes a socket connection to the Blink server.
- `void close_server()`: Closes the connection to the server.

##### **Key-Value Operations**
- `std::string set(const std::string &_key, const std::string &_value)`: Sends a `SET` command to store a key-value pair.
- `std::string get(const std::string &_key)`: Sends a `GET` command to retrieve the value of a key.
- `std::string del(const std::string &_key)`: Sends a `DEL` command to delete a key.

##### **Command Encoding & Decoding**
- `std::string encode_command(const std::string &_input)`: Converts a command string into RESP format.
- `std::string decode_resp(const std::string &_response)`: Parses the RESP response into a human-readable format.

##### **Communication**
- `std::string send_req(const std::string &_encoded)`: Sends an encoded command to the Blink server and retrieves the response.

## **Workflow**
1. The client initializes the connection using `server_init()`.
2. Commands like `SET`, `GET`, and `DEL` are encoded into RESP format using `encode_command()`.
3. The encoded request is sent via `send_req()`, and the response is read into a buffer.
4. The response is decoded using `decode_resp()` and returned to the user.
5. The connection is closed using `close_server()` when no longer needed.

## **Error Handling**
- Connection failures print error messages.
- If the server response is empty, it returns "Empty response".
- If the server disconnects, it returns "Server disconnected".
- RESP decoding handles multiple response types, including strings, errors, integers, and bulk responses.

