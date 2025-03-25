#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cstring>

#include "../lib/lru_cache_v1.h"

// Utility function to generate random strings
std::string generateRandomString(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.reserve(length);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    for (size_t i = 0; i < length; ++i) {
        result += alphanum[dis(gen)];
    }
    return result;
}

// Performance metrics structure
struct PerformanceMetrics {
    double hitRatio;
    double avgGetLatency;
    double avgSetLatency;
    size_t peakMemoryUsage;
    size_t finalCacheSize;
    
    void print() {
        std::cout << "===== Performance Metrics =====" << std::endl;
        std::cout << "Hit Ratio: " << std::fixed << std::setprecision(2) << (hitRatio * 100) << "%" << std::endl;
        std::cout << "Average GET Latency: " << std::fixed << std::setprecision(3) << avgGetLatency << " μs" << std::endl;
        std::cout << "Average SET Latency: " << std::fixed << std::setprecision(3) << avgSetLatency << " μs" << std::endl;
        std::cout << "Peak Memory Usage: " << peakMemoryUsage << " bytes" << std::endl;
        std::cout << "Final Cache Size: " << finalCacheSize << " items" << std::endl;
    }
};

// Benchmark function for random access pattern
PerformanceMetrics benchmarkRandomAccess(size_t cacheSize, size_t numOperations, size_t keySize, size_t valueSize) {
    LRUCache cache(cacheSize);
    
    // Generate a pool of keys and values
    size_t keyPoolSize = numOperations / 5; // 20% unique keys
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    std::cout << "Generating test data..." << std::endl;
    for (size_t i = 0; i < keyPoolSize; ++i) {
        keys.push_back(generateRandomString(keySize));
        values.push_back(generateRandomString(valueSize));
    }
    
    // Performance tracking variables
    size_t hits = 0;
    size_t misses = 0;
    size_t peakMemory = 0;
    double totalGetTime = 0;
    double totalSetTime = 0;
    size_t getOps = 0;
    size_t setOps = 0;
    
    // Random number generation for key selection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> keyDis(0, keyPoolSize - 1);
    std::uniform_int_distribution<> opDis(0, 100);
    
    std::cout << "Running benchmark with " << numOperations << " operations..." << std::endl;
    
    // Run the benchmark
    for (size_t i = 0; i < numOperations; ++i) {
        int keyIndex = keyDis(gen);
        int operation = opDis(gen);
        
        // 70% GET, 30% SET operations
        if (operation < 70) {
            // GET operation
            auto start = std::chrono::high_resolution_clock::now();
            std::string result = cache.get(keys[keyIndex].c_str());
            auto end = std::chrono::high_resolution_clock::now();
            
            if (result != "-1") {
                hits++;
            } else {
                misses++;
            }
            
            totalGetTime += std::chrono::duration<double, std::micro>(end - start).count();
            getOps++;
        } else {
            // SET operation
            auto start = std::chrono::high_resolution_clock::now();
            cache.set(strdup(keys[keyIndex].c_str()), strdup(values[keyIndex].c_str()));
            auto end = std::chrono::high_resolution_clock::now();
            
            totalSetTime += std::chrono::duration<double, std::micro>(end - start).count();
            setOps++;
        }
        
        // Track peak memory usage
        size_t currentMemory = cache.memory_usage();
        peakMemory = std::max(peakMemory, currentMemory);
        
        // Progress indicator
        if (i % (numOperations / 10) == 0) {
            std::cout << "Progress: " << (i * 100 / numOperations) << "%" << std::endl;
        }
    }
    
    // Calculate metrics
    PerformanceMetrics metrics;
    metrics.hitRatio = static_cast<double>(hits) / (hits + misses);
    metrics.avgGetLatency = getOps > 0 ? totalGetTime / getOps : 0;
    metrics.avgSetLatency = setOps > 0 ? totalSetTime / setOps : 0;
    metrics.peakMemoryUsage = peakMemory;
    metrics.finalCacheSize = cache.size();
    
    return metrics;
}

// Benchmark function for sequential access pattern
PerformanceMetrics benchmarkSequentialAccess(size_t cacheSize, size_t numOperations, size_t keySize, size_t valueSize) {
    LRUCache cache(cacheSize);
    
    // Generate a pool of keys and values
    size_t keyPoolSize = numOperations / 5; // 20% unique keys
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    std::cout << "Generating test data..." << std::endl;
    for (size_t i = 0; i < keyPoolSize; ++i) {
        keys.push_back(generateRandomString(keySize));
        values.push_back(generateRandomString(valueSize));
    }
    
    // Performance tracking variables
    size_t hits = 0;
    size_t misses = 0;
    size_t peakMemory = 0;
    double totalGetTime = 0;
    double totalSetTime = 0;
    size_t getOps = 0;
    size_t setOps = 0;
    
    std::cout << "Running benchmark with " << numOperations << " operations..." << std::endl;
    
    // First, populate the cache with sequential keys
    for (size_t i = 0; i < std::min(keyPoolSize, cacheSize / (keySize + valueSize)); ++i) {
        cache.set(strdup(keys[i].c_str()), strdup(values[i].c_str()));
    }
    
    // Run the benchmark with sequential access
    for (size_t i = 0; i < numOperations; ++i) {
        size_t keyIndex = i % keyPoolSize;
        
        // 70% GET, 30% SET operations
        if (i % 10 < 7) {
            // GET operation
            auto start = std::chrono::high_resolution_clock::now();
            std::string result = cache.get(keys[keyIndex].c_str());
            auto end = std::chrono::high_resolution_clock::now();
            
            if (result != "-1") {
                hits++;
            } else {
                misses++;
            }
            
            totalGetTime += std::chrono::duration<double, std::micro>(end - start).count();
            getOps++;
        } else {
            // SET operation
            auto start = std::chrono::high_resolution_clock::now();
            cache.set(strdup(keys[keyIndex].c_str()), strdup(values[keyIndex].c_str()));
            auto end = std::chrono::high_resolution_clock::now();
            
            totalSetTime += std::chrono::duration<double, std::micro>(end - start).count();
            setOps++;
        }
        
        // Track peak memory usage
        size_t currentMemory = cache.memory_usage();
        peakMemory = std::max(peakMemory, currentMemory);
        
        // Progress indicator
        if (i % (numOperations / 10) == 0) {
            std::cout << "Progress: " << (i * 100 / numOperations) << "%" << std::endl;
        }
    }
    
    // Calculate metrics
    PerformanceMetrics metrics;
    metrics.hitRatio = static_cast<double>(hits) / (hits + misses);
    metrics.avgGetLatency = getOps > 0 ? totalGetTime / getOps : 0;
    metrics.avgSetLatency = setOps > 0 ? totalSetTime / setOps : 0;
    metrics.peakMemoryUsage = peakMemory;
    metrics.finalCacheSize = cache.size();
    
    return metrics;
}

int main() {
    // Configuration parameters
    const size_t CACHE_SIZE_SMALL = 10 * 1024 * 1024;  // 1MB
    const size_t CACHE_SIZE_LARGE = 100 * 1024 * 1024; // 10MB
    const size_t NUM_OPERATIONS = 1000000;
    const size_t KEY_SIZE = 16;
    const size_t VALUE_SIZE = 128;
    
    std::cout << "===== LRU Cache Performance Benchmark =====" << std::endl;
    std::cout << "Cache Size (Small): " << CACHE_SIZE_SMALL / 1024 << " KB" << std::endl;
    std::cout << "Cache Size (Large): " << CACHE_SIZE_LARGE / 1024 << " KB" << std::endl;
    std::cout << "Operations: " << NUM_OPERATIONS << std::endl;
    std::cout << "Key Size: " << KEY_SIZE << " bytes" << std::endl;
    std::cout << "Value Size: " << VALUE_SIZE << " bytes" << std::endl;
    std::cout << std::endl;
    
    // Run benchmarks
    std::cout << "===== Random Access Pattern (Small Cache) =====" << std::endl;
    auto randomSmall = benchmarkRandomAccess(CACHE_SIZE_SMALL, NUM_OPERATIONS, KEY_SIZE, VALUE_SIZE);
    randomSmall.print();
    
    std::cout << "\n===== Sequential Access Pattern (Small Cache) =====" << std::endl;
    auto sequentialSmall = benchmarkSequentialAccess(CACHE_SIZE_SMALL, NUM_OPERATIONS, KEY_SIZE, VALUE_SIZE);
    sequentialSmall.print();
    
    std::cout << "\n===== Random Access Pattern (Large Cache) =====" << std::endl;
    auto randomLarge = benchmarkRandomAccess(CACHE_SIZE_LARGE, NUM_OPERATIONS, KEY_SIZE, VALUE_SIZE);
    randomLarge.print();
    
    std::cout << "\n===== Sequential Access Pattern (Large Cache) =====" << std::endl;
    auto sequentialLarge = benchmarkSequentialAccess(CACHE_SIZE_LARGE, NUM_OPERATIONS, KEY_SIZE, VALUE_SIZE);
    sequentialLarge.print();
    
    return 0;
}
