#include <iostream>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>

#include "../lib/dict.h"

// Hash function for C-style strings
unsigned int stringHash(const void* key) {
    const char* str = static_cast<const char*>(key);
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}

// Key comparison function
int stringCompare(const void* key1, const void* key2) {
    return strcmp(static_cast<const char*>(key1), static_cast<const char*>(key2)) == 0;
}

// Key duplication function
void* stringDup(const void* key) {
    return strdup(static_cast<const char*>(key));
}

// Key/value destructor
void freeString(void* ptr) {
    free(ptr);
}

// Timer class for measuring performance
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::string operation_name;

public:
    Timer(const std::string& name) : operation_name(name) {
        start_time = std::chrono::high_resolution_clock::now();
    }

    ~Timer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        std::cout << std::setw(20) << operation_name << ": " 
                  << std::setw(10) << duration << " microseconds" << std::endl;
    }
};

// Extend the Dict class to add rehashing monitoring
class MonitoredDict : public Dict {
private:
    int rehash_count = 0;
    int resize_count = 0;
    bool last_rehashing_state = false;

public:
    MonitoredDict(std::function<unsigned int(const void*)> hashFunc,
                 std::function<void*(const void*)> keyDupFunc,
                 std::function<void*(const void*)> valDupFunc,
                 std::function<int(const void*, const void*)> keyCompareFunc,
                 std::function<void(void*)> keyDestructorFunc,
                 std::function<void(void*)> valDestructorFunc)
        : Dict(hashFunc, keyDupFunc, valDupFunc, keyCompareFunc, keyDestructorFunc, valDestructorFunc) {}

    // Override methods to monitor rehashing
    int add_d(void* key, void* val) {
        bool was_rehashing = isRehashing();
        int result = Dict::add(key, val);
        checkRehashingState(was_rehashing);
        return result;
    }

    void* find_d(const void* key) {
        bool was_rehashing = isRehashing();
        void* result = Dict::find(key);
        checkRehashingState(was_rehashing);
        return result;
    }

    int remove_d(const void* key) {
        bool was_rehashing = isRehashing();
        int result = Dict::remove(key);
        checkRehashingState(was_rehashing);
        return result;
    }

    // Method to check if rehashing is in progress
    bool isRehashing() {
        return Dict::isRehashing();
    }

    // Method to check if rehashing state changed
    void checkRehashingState(bool was_rehashing) {
        bool is_rehashing = isRehashing();
        
        // Detect start of rehashing
        if (!was_rehashing && is_rehashing) {
            resize_count++;
            std::cout << "RESIZE #" << resize_count << ": Rehashing started" << std::endl;
        }
        
        // Count rehash steps
        if (was_rehashing && is_rehashing) {
            rehash_count++;
            if (rehash_count % 1000 == 0) {
                std::cout << "  - " << rehash_count << " rehash steps performed" << std::endl;
            }
        }
        
        // Detect end of rehashing
        if (was_rehashing && !is_rehashing) {
            std::cout << "RESIZE #" << resize_count << " COMPLETE: " 
                      << rehash_count << " total rehash steps performed" << std::endl;
            rehash_count = 0;
        }
        
        last_rehashing_state = is_rehashing;
    }

    // Get statistics
    void printStats() {
        std::cout << "\nRehashing Statistics:" << std::endl;
        std::cout << "Total resize operations: " << resize_count << std::endl;
        std::cout << "Currently rehashing: " << (isRehashing() ? "Yes" : "No") << std::endl;
        if (isRehashing()) {
            std::cout << "Current rehash steps: " << rehash_count << std::endl;
        }
    }
};

int main() {
    // Create monitored dictionary
    MonitoredDict dict(stringHash, stringDup, stringDup, stringCompare, freeString, freeString);
    
    std::cout << "Testing dictionary with automatic incremental rehashing\n";
    std::cout << "======================================================\n\n";
    
    // Generate a large number of keys for testing
    const int NUM_ENTRIES = 100000;
    std::vector<std::string> keys;
    keys.reserve(NUM_ENTRIES);
    
    for (int i = 0; i < NUM_ENTRIES; i++) {
        keys.push_back("key" + std::to_string(i));
    }
    
    // Randomize keys to avoid sequential insertion patterns
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(keys.begin(), keys.end(), g);
    
    // Test 1: Measure insertion time with rehashing monitoring
    {
        Timer timer("Insertion");
        for (int i = 0; i < NUM_ENTRIES; i++) {
            std::string value = "value" + std::to_string(i);
            dict.add_d(strdup(keys[i].c_str()), strdup(value.c_str()));
            
            // Print progress every 10,000 insertions
            if ((i + 1) % 10000 == 0) {
                std::cout << "Inserted " << (i + 1) << " entries" << std::endl;
                // Print current dictionary state
                if (dict.isRehashing()) {
                    std::cout << "  - Currently rehashing" << std::endl;
                }
            }
        }
    }
    
    // Print rehashing statistics after insertion
    dict.printStats();
    
    // Test 2: Measure lookup time for existing keys
    {
        Timer timer("Lookup (existing)");
        int found_count = 0;
        for (int i = 0; i < NUM_ENTRIES; i++) {
            void* val = dict.find_d(keys[i].c_str());
            if (val) found_count++;
            
            // Print progress and rehashing status every 10,000 lookups
            if ((i + 1) % 10000 == 0) {
                std::cout << "Looked up " << (i + 1) << " entries" << std::endl;
                if (dict.isRehashing()) {
                    std::cout << "  - Currently rehashing during lookup" << std::endl;
                }
            }
        }
        std::cout << "Found " << found_count << " out of " << NUM_ENTRIES << " keys" << std::endl;
    }
    
    // Print rehashing statistics after lookup
    dict.printStats();
    
    // Test 3: Measure deletion time with rehashing monitoring
    {
        // Delete half of the keys
        const int DELETE_COUNT = NUM_ENTRIES / 2;
        Timer timer("Deletion");
        int deleted_count = 0;
        for (int i = 0; i < DELETE_COUNT; i++) {
            if (dict.remove_d(keys[i].c_str()) == 0) {
                deleted_count++;
            }
            
            // Print progress every 10,000 deletions
            if ((i + 1) % 10000 == 0) {
                std::cout << "Deleted " << (i + 1) << " entries" << std::endl;
                if (dict.isRehashing()) {
                    std::cout << "  - Currently rehashing during deletion" << std::endl;
                }
            }
        }
        std::cout << "Successfully deleted " << deleted_count << " out of " << DELETE_COUNT << " keys" << std::endl;
    }
    
    // Print final rehashing statistics
    dict.printStats();
    
    // Test 4: Force rehashing with high load factor
    {
        // Create a new monitored dictionary with resize disabled initially
        MonitoredDict dict_forced(stringHash, stringDup, stringDup, stringCompare, freeString, freeString);
        dict_forced.enableResize(false);
        
        std::cout << "\nTesting forced rehashing with high load factor:" << std::endl;
        
        // Insert entries to reach high load factor
        for (int i = 0; i < 10000; i++) {
            std::string value = "value" + std::to_string(i);
            dict_forced.add_d(strdup(keys[i].c_str()), strdup(value.c_str()));
            
            // Print progress every 1,000 insertions
            if ((i + 1) % 1000 == 0) {
                std::cout << "Inserted " << (i + 1) << " entries (resize disabled)" << std::endl;
            }
        }
        
        // Now enable resize and insert one more entry to trigger forced rehashing
        std::cout << "\nEnabling resize with high load factor..." << std::endl;
        dict_forced.enableResize(true);
        
        Timer timer("Forced rehashing");
        std::string value = "trigger_value";
        dict_forced.add_d(strdup("trigger_key"), strdup(value.c_str()));
        
        // Perform some lookups to ensure rehashing continues
        for (int i = 0; i < 5000; i++) {
            dict_forced.find_d(keys[i].c_str());
            
            // Print progress every 1,000 lookups
            if ((i + 1) % 1000 == 0) {
                std::cout << "Performed " << (i + 1) << " lookups during forced rehashing" << std::endl;
                if (dict_forced.isRehashing()) {
                    std::cout << "  - Still rehashing..." << std::endl;
                } else {
                    std::cout << "  - Rehashing completed" << std::endl;
                }
            }
        }
        
        // Print final statistics for forced rehashing
        dict_forced.printStats();
    }
    
    std::cout << "\nTest completed successfully!" << std::endl;
    
    return 0;
}
