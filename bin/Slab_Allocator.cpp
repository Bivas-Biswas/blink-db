#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <ctime>
#include <string>
#include <algorithm>
#include <array>
#include <chrono>

class SlabAllocator {
private:
    static constexpr size_t PAGE_SIZE = 1024 * 1024; // 1MB page size
    static constexpr size_t MIN_CHUNK_SIZE = 80;     // Minimum chunk size
    static constexpr float GROWTH_FACTOR = 1.25f;    // Growth factor
    static constexpr size_t MAX_ITEMS = 1000000;     // Maximum number of items

    struct Chunk {
        void* memory;
        size_t size;
        bool used;
        std::string key;
        time_t last_accessed;
        
        Chunk(void* mem, size_t sz) : memory(mem), size(sz), used(false), last_accessed(0) {}
    };

    struct Page {
        void* memory;
        std::vector<Chunk> chunks;
        size_t chunk_size;
        size_t free_chunks;
        int slab_class;
        
        Page(void* mem, size_t cs, int sc) : 
            memory(mem), 
            chunk_size(cs), 
            free_chunks(PAGE_SIZE / cs),
            slab_class(sc) {
            
            // Initialize chunks
            for (size_t i = 0; i < free_chunks; i++) {
                void* chunk_mem = static_cast<char*>(memory) + (i * chunk_size);
                chunks.emplace_back(chunk_mem, chunk_size);
            }
        }
    };

    struct SlabClass {
        size_t chunk_size;
        std::vector<Page*> pages;
        
        SlabClass(size_t cs) : chunk_size(cs) {}
        
        // Find a free chunk in any page of this slab class
        Chunk* findFreeChunk() {
            for (Page* page : pages) {
                for (Chunk& chunk : page->chunks) {
                    if (!chunk.used) {
                        return &chunk;
                    }
                }
            }
            return nullptr;
        }
        
        // Find the least recently used chunk
        Chunk* findLRUChunk() {
            Chunk* lru = nullptr;
            time_t oldest_time = time(nullptr);
            
            for (Page* page : pages) {
                for (Chunk& chunk : page->chunks) {
                    if (chunk.used && chunk.last_accessed < oldest_time) {
                        oldest_time = chunk.last_accessed;
                        lru = &chunk;
                    }
                }
            }
            
            return lru;
        }
    };

    // Fast hash function for strings
    size_t hashString(const std::string& str) const {
        // FNV-1a hash
        size_t hash = 14695981039346656037ULL; // FNV offset basis
        for (char c : str) {
            hash ^= static_cast<size_t>(c);
            hash *= 1099511628211ULL; // FNV prime
        }
        return hash;
    }

    // Low-latency key-value store using open addressing with linear probing
    class FastKeyValueStore {
    private:
        struct Entry {
            std::string key;
            Chunk* value;
            bool used;
            
            Entry() : value(nullptr), used(false) {}
        };
        
        std::vector<Entry> entries;
        size_t size_;
        size_t capacity_;
        float load_factor_threshold;
        
    public:
        FastKeyValueStore(size_t initial_capacity = 16384, float load_factor = 0.75f) 
            : size_(0), capacity_(initial_capacity), load_factor_threshold(load_factor) {
            entries.resize(capacity_);
        }

        // Hash function for strings
        size_t hashString(const std::string& str) const {
            size_t hash = 5381;
            for (char c : str) {
                hash = ((hash << 5) + hash) + c; // hash * 33 + c
            }
            return hash;
        }
        
        void resize(size_t new_capacity) {
            std::vector<Entry> old_entries = std::move(entries);
            entries.resize(new_capacity);
            capacity_ = new_capacity;
            size_ = 0;
            
            for (const Entry& entry : old_entries) {
                if (entry.used) {
                    insert(entry.key, entry.value);
                }
            }
        }
        
        void insert(const std::string& key, Chunk* value) {
            if (size_ >= capacity_ * load_factor_threshold) {
                resize(capacity_ * 2);
            }
            
            size_t hash = hashString(key) % capacity_;
            size_t index = hash;
            
            while (entries[index].used && entries[index].key != key) {
                index = (index + 1) % capacity_;
                if (index == hash) { // Full table
                    resize(capacity_ * 2);
                    insert(key, value);
                    return;
                }
            }
            
            if (!entries[index].used) {
                size_++;
            }
            
            entries[index].key = key;
            entries[index].value = value;
            entries[index].used = true;
        }
        
        Chunk* find(const std::string& key) const {
            size_t hash = hashString(key) % capacity_;
            size_t index = hash;
            
            while (entries[index].used) {
                if (entries[index].key == key) {
                    return entries[index].value;
                }
                
                index = (index + 1) % capacity_;
                if (index == hash) {
                    break;
                }
            }
            
            return nullptr;
        }
        
        bool erase(const std::string& key) {
            size_t hash = hashString(key) % capacity_;
            size_t index = hash;
            
            while (entries[index].used) {
                if (entries[index].key == key) {
                    // Mark as deleted
                    entries[index].used = false;
                    entries[index].key.clear();
                    entries[index].value = nullptr;
                    size_--;
                    
                    // Re-hash entries that might have been placed after this one
                    size_t next_index = (index + 1) % capacity_;
                    while (entries[next_index].used) {
                        Entry temp = std::move(entries[next_index]);
                        entries[next_index].used = false;
                        size_--;
                        insert(temp.key, temp.value);
                        next_index = (next_index + 1) % capacity_;
                    }
                    
                    return true;
                }
                
                index = (index + 1) % capacity_;
                if (index == hash) {
                    break;
                }
            }
            
            return false;
        }
        
        size_t size() const {
            return size_;
        }
    };

    std::vector<SlabClass> slab_classes;
    std::vector<void*> memory_blocks;
    FastKeyValueStore items;
    size_t total_memory;
    size_t free_pages;

    // Calculate the appropriate slab class for a given size
    int getSlabClass(size_t size) {
        for (size_t i = 0; i < slab_classes.size(); i++) {
            if (size <= slab_classes[i].chunk_size) {
                return i;
            }
        }
        return -1; // Too large
    }

    // Allocate a new page for a slab class
    bool allocateNewPage(int slab_class) {
        if (free_pages == 0) {
            return false;
        }
        
        // Find a free memory block
        void* page_memory = nullptr;
        for (void* block : memory_blocks) {
            bool used = false;
            for (const SlabClass& sc : slab_classes) {
                for (Page* page : sc.pages) {
                    if (page->memory == block) {
                        used = true;
                        break;
                    }
                }
                if (used) break;
            }
            
            if (!used) {
                page_memory = block;
                break;
            }
        }
        
        if (!page_memory) {
            return false;
        }
        
        // Create a new page
        Page* new_page = new Page(page_memory, slab_classes[slab_class].chunk_size, slab_class);
        slab_classes[slab_class].pages.push_back(new_page);
        free_pages--;
        
        return true;
    }


public:
    SlabAllocator(size_t memory_limit, float growth_factor = GROWTH_FACTOR, size_t min_chunk_size = MIN_CHUNK_SIZE) 
        : items(MAX_ITEMS / 4) { // Initialize with a reasonable size
        total_memory = memory_limit;
        free_pages = memory_limit / PAGE_SIZE;
        
        // Allocate memory blocks (pages)
        for (size_t i = 0; i < free_pages; i++) {
            void* block = aligned_alloc(64, PAGE_SIZE); // Align to cache line for better performance
            if (!block) {
                throw std::bad_alloc();
            }
            memory_blocks.push_back(block);
        }
        
        // Initialize slab classes
        size_t chunk_size = min_chunk_size;
        while (chunk_size <= PAGE_SIZE) {
            slab_classes.emplace_back(chunk_size);
            
            // Calculate next chunk size using growth factor
            chunk_size = std::ceil(chunk_size * growth_factor);
            
            // Round to next power of 2 if needed
            if (chunk_size > 512) {
                size_t power_of_two = 1;
                while (power_of_two < chunk_size) {
                    power_of_two *= 2;
                }
                chunk_size = power_of_two;
            }
        }
        
        // Initialize one page per slab class
        for (size_t i = 0; i < slab_classes.size() && free_pages > 0; i++) {
            allocateNewPage(i);
        }
    }
    
    ~SlabAllocator() {
        // Free all memory blocks
        for (void* block : memory_blocks) {
            free(block);
        }
    }
    
    // Store an item in the slab allocator
    bool set(const std::string& key, const void* data, size_t size) {
        // Check if we're updating an existing item
        Chunk* chunk = items.find(key);
        if (chunk) {
            // If new data fits in the existing chunk
            if (size <= chunk->size) {
                memcpy(chunk->memory, data, size);
                chunk->last_accessed = time(nullptr);
                return true;
            } else {
                // Remove the old item if we need a larger chunk
                chunk->used = false;
                chunk->key.clear();
                items.erase(key);
            }
        }
        
        // Find appropriate slab class
        int slab_class = getSlabClass(size);
        if (slab_class == -1) {
            return false; // Item too large
        }
        
        // Find a free chunk
        Chunk* new_chunk = slab_classes[slab_class].findFreeChunk();
        
        // If no free chunk, try to allocate a new page
        if (!new_chunk && free_pages > 0) {
            if (allocateNewPage(slab_class)) {
                new_chunk = slab_classes[slab_class].findFreeChunk();
            }
        }
        
        // If still no free chunk, evict using LRU
        if (!new_chunk) {
            new_chunk = slab_classes[slab_class].findLRUChunk();
            if (new_chunk) {
                // Remove the old item from the hash table
                items.erase(new_chunk->key);
                new_chunk->used = false;
                new_chunk->key.clear();
            }
        }
        
        if (!new_chunk) {
            return false; // No space available
        }
        
        // Store the item
        memcpy(new_chunk->memory, data, size);
        new_chunk->used = true;
        new_chunk->key = key;
        new_chunk->last_accessed = time(nullptr);
        items.insert(key, new_chunk);
        
        return true;
    }
    
    // Retrieve an item from the slab allocator
    bool get(const std::string& key, void* buffer, size_t& size) {
        Chunk* chunk = items.find(key);
        if (!chunk) {
            return false;
        }
        
        memcpy(buffer, chunk->memory, chunk->size);
        size = chunk->size;
        chunk->last_accessed = time(nullptr);
        
        return true;
    }
    
    // Delete an item from the slab allocator
    bool remove(const std::string& key) {
        Chunk* chunk = items.find(key);
        if (!chunk) {
            return false;
        }
        
        chunk->used = false;
        chunk->key.clear();
        items.erase(key);
        
        return true;
    }
    
    // Print statistics about the slab allocator
    void printStats() {
        std::cout << "Slab Allocator Statistics:" << std::endl;
        std::cout << "Total Memory: " << total_memory << " bytes" << std::endl;
        std::cout << "Free Pages: " << free_pages << std::endl;
        std::cout << "Slab Classes: " << slab_classes.size() << std::endl;
        std::cout << "Total Items: " << items.size() << std::endl;
        
        for (size_t i = 0; i < slab_classes.size(); i++) {
            const SlabClass& sc = slab_classes[i];
            size_t total_chunks = 0;
            size_t used_chunks = 0;
            
            for (const Page* page : sc.pages) {
                total_chunks += page->chunks.size();
                for (const Chunk& chunk : page->chunks) {
                    if (chunk.used) {
                        used_chunks++;
                    }
                }
            }
            
            std::cout << "Slab Class " << i << ": " << std::endl;
            std::cout << "  Chunk Size: " << sc.chunk_size << " bytes" << std::endl;
            std::cout << "  Pages: " << sc.pages.size() << std::endl;
            std::cout << "  Total Chunks: " << total_chunks << std::endl;
            std::cout << "  Used Chunks: " << used_chunks << " (" 
                      << (total_chunks > 0 ? (used_chunks * 100.0 / total_chunks) : 0) 
                      << "%)" << std::endl;
        }
    }
};

int main() {
    // Create a slab allocator with 10MB of memory
    SlabAllocator allocator(10 * 1024 * 1024);
    
    // Example usage
    const char* data1 = "This is a test string";
    allocator.set("key1", data1, strlen(data1) + 1);
    
    char buffer[256];
    size_t size = sizeof(buffer);
    if (allocator.get("key1", buffer, size)) {
        std::cout << "Retrieved: " << buffer << std::endl;
    }
    
    // Store a larger object
    std::vector<int> numbers(1000, 42);
    allocator.set("key2", numbers.data(), numbers.size() * sizeof(int));
    
    // Print statistics
    allocator.printStats();
    
    // Benchmark
    const int NUM_ITEMS = 100000;
    std::cout << "Inserting " << NUM_ITEMS << " items..." << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_ITEMS; i++) {
        std::string key = "benchmark_key_" + std::to_string(i);
        int value = i;
        allocator.set(key, &value, sizeof(value));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "Time to insert " << NUM_ITEMS << " items: " << duration << "ms" << std::endl;
    std::cout << "Average insertion time: " << static_cast<double>(duration) / NUM_ITEMS << "ms per item" << std::endl;
    
    // Test retrieval
    start = std::chrono::high_resolution_clock::now();
    
    int hits = 0;
    for (int i = 0; i < NUM_ITEMS; i++) {
        std::string key = "benchmark_key_" + std::to_string(i);
        int value;
        size_t value_size = sizeof(value);
        if (allocator.get(key, &value, value_size)) {
            hits++;
            if (value != i) {
                std::cout << "Error: value mismatch for key " << key << std::endl;
            }
        }
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "Time to retrieve " << NUM_ITEMS << " items: " << duration << "ms" << std::endl;
    std::cout << "Average retrieval time: " << static_cast<double>(duration) / NUM_ITEMS << "ms per item" << std::endl;
    std::cout << "Hit rate: " << (static_cast<double>(hits) / NUM_ITEMS * 100) << "%" << std::endl;
    
    return 0;
}
