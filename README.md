# **BlinkDB: A High-Performance In-Memory Key-Value Database**

## **📖 Overview**

BlinkDB is a high-performance, in-memory key-value database inspired by **Redis**. It is implemented in C++ and designed to be a cohesive system that integrates a sophisticated storage engine with a networked TCP server layer.

It can operate as a **standalone server** or as a **distributed system** with a load balancer. The server is designed to be highly efficient, managing thousands of concurrent client connections on a **single thread** using epoll for I/O multiplexing. Communication with clients is handled using the Redis wire protocol (**RESP-2**).

## **✨ Features**

* **In-Memory Storage**: A highly optimized in-memory hash table provides extremely fast read and write operations.  
* **Data Persistence**: An optional persistence layer saves data to disk, ensuring durability across restarts.  
* **LRU Cache**: Implements a Least Recently Used (LRU) eviction policy to manage memory usage efficiently.  
* **Asynchronous I/O**: Uses epoll to handle thousands of concurrent client connections without blocking, ensuring high throughput on a single thread.  
* **Distributed Architecture**: A load balancer can distribute traffic across multiple BlinkDB server instances for horizontal scaling, using **consistent hashing** to route requests.  
* **Redis Compatible**: Communicates using the RESP-2 protocol, making it compatible with standard tools like redis-benchmark.

## **🏗️ Architecture**

BlinkDB's architecture is composed of several key components that work together to provide a fast and reliable database system.

1. **Dictionary (Hash Table)** 🤓: The core of the in-memory storage. It uses separate chaining for collision resolution and incremental rehashing to avoid latency spikes during resizing.  
2. **PersistenceKVStore** 💾: For data durability, this component writes data to an append-only file on disk. A Trie-based index and a BloomFilter are used to make disk lookups fast and efficient.  
3. **LRU Cache** 🧠: Manages the in-memory data by evicting the least recently used items to the PersistenceKVStore when memory limits are reached.  
4. **TCP Server & Event Loop** 🌐: The server listens for client connections and uses an epoll-based event loop to handle I/O asynchronously, allowing a single thread to manage many connections at once.  
5. **Load Balancer (Distributed Mode)** 🚀: When running in distributed mode, the load balancer acts as a single entry point. It uses a consistent hashing ring to route client requests to the appropriate backend server instance.

## **🛠️ How to Build and Run**

This project uses a Makefile to simplify the build and execution process.

### **Build the Project**

To compile all binaries (blink\_server, blink\_cli, and blink\_server\_with\_lb), run the all target. The binaries will be placed in the `build/` directory.
```
make all
```

### **Run Standalone Server**

To run a single instance of the BlinkDB server:
```
make run_server
```

The server will start listening on port 9001\.

### **Run CLI Client**

To interact with the server, you can use the provided command-line client:

```
make run_client
```

You can then issue commands like SET, GET, and DEL.

```
\> SET mykey myvalue  
OK  
\> GET mykey  
myvalue
```


### **Run Distributed Server with Load Balancer**

To run BlinkDB in a distributed mode with multiple server instances managed by a load balancer:
```
make run_lb_server
```
This will prompt you to enter the number of backend servers you want to run. The load balancer will then start on port 9001 and distribute requests to the server instances.

### **Clean**

To remove the build directory and all compiled binaries:

make clean

## **🚀 Benchmarking**

You can test the server's performance using redis-benchmark.
```
redis-benchmark -h 127.0.0.1 -p 9001 -t set,get,del -n 100000 -c 100 -r 1000
```

### **Expected Performance**

Based on benchmark tests, a single BlinkDB server instance can achieve excellent throughput. Performance is highest with fewer parallel clients (e.g., 10) and can reach over **87k req/sec**. Throughput decreases as client concurrency increases, which is a typical characteristic of a single-threaded architecture.

| Total Requests | Parallel Clients | SET (RPS) | GET (RPS) |
| :---- | :---- | :---- | :---- |
| **100,000** | 10 | 85,763.29 | 87,260.03 |
| **100,000** | 100 | 79,239.30 | 83,822.30 |
| **100,000** | 1,000 | 57,971.02 | 53,361.79 |

## **📄 Documentation**

Detailed design documents and API documentation are available in the docs/ directory.

* **Design Document**: [docs/DESIGN_DOC.md](docs/DESIGN_DOC.md)
* **Benchmark Document**: [docs/PERFORMANCE_BENCHMARK.md](docs/PERFORMANCE_BENCHMARK.md)
* **Doxygen PDF**: [docs/latex/refman.pdf](docs/latex/refman.pdf) (after generation)  
* **Doxygen HTML**: Open [docs/html/index.html](docs/html/index.html) in your browser.

To generate (or regenerate) the Doxygen documentation, run:

make doxygen

This will clean the docs directory, run Doxygen to generate HTML and LaTeX files, and then compile the LaTeX to produce a PDF.

## **📁 Project Structure**

.  
├── build/            \# Compiled binaries  
├── docs/             \# Design documents and Doxygen output  
├── lib/              \# Core library files (server, cache, etc.)  
├── src/              \# Source code for executables  
├── utils/            \# Utility functions  
├── Doxyfile          \# Doxygen configuration  
└── Makefile          \# Build script  