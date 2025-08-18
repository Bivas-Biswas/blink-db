### **\#\# Detailed Summary of the BlinkDB Project**

BlinkDB is a high-performance, in-memory key-value database inspired by Redis. It is implemented in C++ as a cohesive system that integrates a sophisticated storage engine with a networked TCP server layer. Though developed in stages, the final product operates as a single, standalone system that efficiently manages multiple client connections using I/O multiplexing with epoll and communicates via the Redis RESP-2 protocol.

---

### **\#\# Dictionary (Hash Table) Design**

The core of the in-memory storage is a highly optimized dictionary (hash table) designed for efficiency and dynamic scaling.

* **Core Data Structures**: The implementation is built upon three main structures:  
  * `dictEntry`: Represents a key-value pair and includes a next pointer to handle hash collisions through chaining.

  * `dictht`: This is the hash table itself, containing an array of dictEntry pointers (the table) and metadata such as its size and the number of used slots.

  * `dict`: The main dictionary structure that holds two dictionary hash tables to facilitate incremental rehashing without blocking operations.

* **Collision Handling**: Collisions are resolved using **separate chaining**, where entries that hash to the same bucket are stored in a linked list.  
* **Dynamic Resizing and Incremental Rehashing**: To maintain performance as the data grows, the hash table supports dynamic resizing. When the load factor exceeds a certain threshold, the table expands. This process uses

  **incremental rehashing**, which moves entries from the old table to the new one gradually over time, avoiding the latency spikes associated with resizing the entire table at once. This ensures that database operations remain fast even during resizing.

---

### **\#\# PersistenceKVStore Design Document**

To ensure data durability beyond the in-memory cache, BlinkDB incorporates a

PersistenceKVStore.

* **Architecture**: This component is responsible for persistently storing key-value pairs on disk. It combines a Trie-based index for fast lookups, a BloomFilter to reduce unnecessary disk access, and a data file for storage. Thread safety is ensured through the use of std::mutex.  
* **Key Components**:  
  * **Trie**: An in-memory Trie data structure is used to index keys and store the byte offset of their corresponding values in the data file, enabling efficient retrieval.  
  * **Bloom Filter**: Before performing a disk lookup, the BloomFilter is checked to probabilistically determine if a key exists. This minimizes slow disk I/O for keys that are not in the database.  
  * **Data File**: Key-value pairs are appended to a text file. While simple, this design is made efficient by the Trie index.  
  * **Background Rewriting**: To manage the append-only nature of the data file, a separate thread periodically performs a rewrite. This process creates a new, compact data file containing only the current, valid entries, thus reclaiming space from deleted or updated records.  
* **Operations**:  
  * **Insertion**: A new key-value pair is appended to the data file, its offset is added to the Trie, and the key is inserted into the BloomFilter.  
  * **Retrieval**: The BloomFilter is checked first. If it indicates the key may be present, the Trie is searched for the file offset, and the value is read from the disk.  
  * **Deletion**: A key is marked as deleted in the Trie and "removed" from the BloomFilter. The actual data remains on disk until the next background rewrite.

---

### **\#\# LRU Cache**

BlinkDB uses an LRU (Least Recently Used) cache to manage in-memory data and ensure that the most relevant data is kept in memory while older data is flushed to the persistent store.

* **Hybrid Structure**: The LRU cache is implemented using two main data structures to achieve both fast lookups and efficient eviction:  
  * A **Dictionary (Hash Table)** for O(1) average time complexity lookups of keys.

  * A  
    **Doubly Linked List** to maintain the order of key access. The most recently used item is at the head, and the least recently used is at the tail.

* **Eviction Policy**: When the cache reaches its memory capacity, the system evicts the least recently used item, which is located at the tail of the linked list. This evicted item is written to the

  PersistenceKVStore to ensure it is not lost.

* **Operations**:  
  * **Get**: When a key is accessed, it is retrieved from the hash table and its corresponding node in the doubly linked list is moved to the head. If the key is not in the cache, it is retrieved from the

    PersistenceKVStore and added to the cache.

  * **Set**: When a key-value pair is added, it is inserted into the hash table and a new node is added to the head of the linked list. If the cache is full, the item at the tail is evicted to the persistent store before the new item is added.

---

### **\#\# Event Loop Handling (epoll-based)**

The server component of BlinkDB is designed to handle a large number of concurrent client connections efficiently without using a thread-per-client model.

* **I/O Multiplexing with epoll**: The server uses the epoll API, a scalable I/O event notification mechanism available on Linux. This allows the single-threaded server to monitor multiple file descriptors (sockets) simultaneously.

* **Event Loop**: The server operates on an event loop. It calls

  epoll\_wait() to block until one or more registered sockets are ready for I/O operations (e.g., a new client connection or incoming data).

* **Non-blocking Sockets**: All client sockets are set to non-blocking mode. This ensures that read/write operations do not lock the server, allowing it to remain responsive and handle other clients while waiting for I/O to complete.

* **Connection Handling**:  
  * When a **new connection** is detected, the server accepts it, makes the new client socket non-blocking, and adds it to the epoll interest list.

  * When **data arrives** from a client, the server reads the request, parses the RESP-2 command, executes it using the storage engine, and sends the response back to the client.

  * The use of epoll ensures that the server only spends time on active connections, making it highly scalable and capable of handling thousands of parallel connections on a single thread.

---

### **\#\# Distributed BlinkDB**

The system can be extended into a distributed architecture using a load balancer that acts as a single entry point for all clients. This load balancer utilizes **consistent hashing** to intelligently route incoming requests to the appropriate backend server, ensuring that requests for the same key consistently go to the same server.

* **Load Balancer**: A central LoadBalancer class is implemented to manage and distribute traffic. It listens for client connections on a main port and uses an epoll-based event loop to handle multiple connections asynchronously without blocking.  
* **Multiple Server Instances**: The application starts by launching a user-defined number of BlinkDB Server instances, each running in its own thread and listening on a different port. These servers function as independent key-value stores.  
* **Consistent Hashing for Request Routing**:  
  * To distribute requests, the load balancer uses a **hash ring**. Each backend server is hashed and placed on this ring.  
  * When a client sends a command (e.g., SET, GET), the load balancer parses the command to extract the **key**.  
  * It then hashes this key and finds the next server on the hash ring. This determines which backend server is responsible for that key.  
  * This consistent hashing approach ensures that the load is evenly distributed and minimizes the number of keys that need to be remapped if a server is added or removed.  
* **Request Forwarding**: Once the target server is identified, the load balancer forwards the original client request to it. After the backend server processes the request and sends back a response, the load balancer relays this response to the client. This entire process is transparent to the client, which only communicates with the load balancer.