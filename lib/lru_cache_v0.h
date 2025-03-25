#ifndef LRU_CACHE_BLINK_H
#define LRU_CACHE_BLINK_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <list>       // For implementing LRU
#include <chrono>     // For timestamp

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
            
            // std::cout << "Memory usage: " << current_memory_usage << "/" << max_memory_bytes 
            //           << " bytes (" << (current_memory_usage * 100.0 / max_memory_bytes) << "%)" << std::endl;
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

#endif