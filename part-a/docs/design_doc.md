# PersistenceKVStore Design Document

## Overview
`PersistenceKVStore` is a persistent key-value storage system that supports efficient data retrieval and background maintenance operations. It utilizes a `Trie`-based indexing mechanism and a `BloomFilter` for fast lookups while ensuring data persistence through file-based storage.

## Features
- **Persistent Storage**: Stores key-value pairs in a file for long-term durability.
- **Efficient Indexing**: Uses a `Trie` structure for fast key searches.
- **Quick Existence Checks**: Implements a `BloomFilter` to quickly determine if a key is likely present.
- **Background Maintenance**: Periodic rewriting to clean up deleted keys and reduce storage overhead.
- **Thread Safety**: Uses `std::mutex` to synchronize access to critical resources.

## Architecture
### Components
- **Trie**: Maintains an in-memory index for fast retrieval of key offsets in the data file.
- **BloomFilter**: Provides probabilistic membership testing to avoid unnecessary file lookups.
- **Data File**: Stores key-value pairs in a simple text format.
- **Rewrite Scheduler**: Periodically rewrites the database file to remove stale entries.

### Class Diagram
```
+-----------------------+
|  PersistenceKVStore  |
+-----------------------+
| - Trie* index        |
| - BloomFilter filter |
| - std::fstream file  |
| - std::string name   |
| - std::mutex mtx     |
| - std::thread rewriteThread |
| - std::atomic<bool> stopRewrite |
| - int rewrite_interval_ms |
+-----------------------+
| + insert(key, value) |
| + get(key, value&)   |
| + remove(key)        |
| + remove_db()        |
| - syncIndex()        |
| - startRewriteScheduler() |
| - triggerRewrite()   |
+-----------------------+
```

## Implementation Details
### Data Storage
- The database file stores key-value pairs in the format: `<key> <value>\n`.
- The `Trie` indexes keys by storing their byte offset in the file.
- The `BloomFilter` is used to determine probable existence before searching the `Trie`.

### Insertion
1. Seek to the end of the file.
2. Append `<key> <value>\n`.
3. Store the key’s offset in the `Trie`.
4. Mark the key as present in the `BloomFilter`.

### Retrieval
1. Check the `BloomFilter`. If absent, return `false`.
2. Query the `Trie` for the key's file offset.
3. Seek to the offset and read the value.

### Deletion
1. Remove the key from the `Trie`.
2. Mark it as removed in the `BloomFilter` (though Bloom filters don't support deletions well).
3. Data remains in the file until rewritten.

### Background Rewrite
- Runs in a separate thread.
- Copies valid entries to a temporary file.
- Swaps the new file with the old one.
- Updates the `Trie` index.

## Performance Considerations
- **Read Efficiency**: Fast due to `Trie` and `BloomFilter`.
- **Write Efficiency**: Slightly impacted by file appends and periodic rewrites.
- **Space Complexity**: Rewrite operation ensures storage efficiency.

## Conclusion
`PersistenceKVStore` is a robust key-value storage system that balances performance and persistence using efficient indexing, background maintenance, and probabilistic filtering techniques.

# Dictionary (Hash Table) Design Document

## Overview
This document describes the design and implementation of a dictionary (hash table) with rehashing support. The hash table is designed to efficiently store key-value pairs, support dynamic resizing, and allow incremental rehashing.

## Data Structures

### `dictEntry`
Represents a single key-value pair in the hash table, supporting multiple data types.

```cpp
struct dictEntry {
    void *key;
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    dictEntry *next; // Chaining for collision handling
};
```

### `dictht`
Represents a hash table with an array of buckets and metadata.

```cpp
struct dictht {
    dictEntry **table;  // Array of hash table entries
    unsigned long size; // Table size
    unsigned long sizemask; // Mask for efficient indexing
    unsigned long used; // Number of occupied slots
};
```

### `dict`
The main dictionary structure that supports rehashing by maintaining two hash tables.

```cpp
struct dict {
    dictht ht[2]; // Two hash tables for rehashing
    long rehashidx; // Index for rehashing (-1 if not rehashing)
    unsigned long iterators; // Number of active iterators
};
```

## Design Decisions

### Hashing Strategy
- A user-defined hash function is used for key hashing.
- Keys are distributed across buckets using `hash % sizemask`.

### Collision Handling
- Uses separate chaining (linked lists) to resolve hash collisions.

### Resizing and Rehashing
- Resizing occurs when the load factor exceeds a threshold.
- Uses incremental rehashing to avoid large latency spikes.
- Rehashing moves entries one step at a time while allowing normal operations.

### Memory Management
- Supports custom key and value destructors for cleanup.
- Uses dynamic memory allocation for hash tables and entries.

## API Functions

### Initialization
```cpp
void _dictInit(dict *d);
```
Initializes an empty dictionary.

### Insertion
```cpp
int dictAdd(dict *d, void *key, void *val);
```
Adds a key-value pair to the dictionary.

### Lookup
```cpp
void *dictFind(dict *d, const void *key);
```
Finds a key and returns its associated value.

### Deletion
```cpp
int dictDelete(dict *d, const void *key);
```
Removes a key from the dictionary.

### Resizing
```cpp
int _dictExpand(dict *d, unsigned long size);
```
Expands the dictionary to a new size.

### Rehashing
```cpp
int dictRehash(dict *d, int n);
```
Performs incremental rehashing of `n` elements.

## Performance Considerations
- Lookup: **O(1) average**.
- Insertion: **O(1) average**.
- Deletion: **O(1) average**.
- Rehashing: **O(n)** total, **O(1)** incremental.


## Conclusion
This dictionary implementation provides an efficient, dynamically resizable hash table with rehashing capabilities. It balances performance and memory usage while offering flexibility through custom function pointers for key-value management.


# Bloom Filter Design Document

## Overview
The `BloomFilter` class implements a simple Bloom filter to provide fast key existence checks. The filter is based on a bitset and utilizes a hash function to determine element presence probabilistically.

## Design Goals
- **Fast Membership Testing**: The Bloom filter allows quick checks to determine if a key is possibly present.
- **Low Memory Usage**: The filter is implemented using a `std::vector<bool>`, which efficiently stores bits.
- **Efficient Insertions**: Adding elements is performed in constant time.
- **Probabilistic Membership**: False positives are possible, but false negatives are not.

## Class Definition
```cpp
class BloomFilter
{
private:
    std::vector<bool> filter; ///< Dynamic bitset for the Bloom filter.
    int filter_size;          ///< Size of the Bloom filter.

public:
    explicit BloomFilter(int _size = 10000);
    void insert(const std::string &_key);
    bool contains(const std::string &_key);
    void remove(const std::string &_key);

private:
    size_t hashKey(const std::string &_key);
};
```

## Components and Functionality
### 1. **Filter Storage**
- Uses a `std::vector<bool>` to store bit values, representing whether an element might be present.
- The size of the filter is configurable via the constructor.

### 2. **Hashing Mechanism**
- Uses `std::hash<std::string>` and `std::hash<std::size_t>` to generate a bit index.
- The final index is computed using modulo operation (`% filter_size`).

### 3. **Insertion**
- The `insert` function computes the hash index and sets the corresponding bit to `true`.

### 4. **Membership Check**
- The `contains` function checks whether the computed hash index is set to `true`.
- A `true` result means the key *may* be present (false positives possible).

### 5. **Removal (Non-Standard Operation)**
- The `remove` function clears a bit, but this can introduce false negatives, making it a non-standard Bloom filter feature.

## Trade-offs and Limitations
- **False Positives**: The filter can mistakenly indicate an element is present when it is not.
- **No True Deletions**: Traditional Bloom filters do not support removal, and clearing a bit may affect multiple keys.
- **Single Hash Function**: Using multiple hash functions could improve accuracy.

## Conclusion
This implementation provides a simple and efficient Bloom filter for fast key existence checks. While it does not support perfect removals and has a risk of false positives, it remains a lightweight solution for many applications.



