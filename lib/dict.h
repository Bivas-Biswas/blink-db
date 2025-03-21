/**
 * @file dict.h
 * @brief Implementation of a dictionary (hash table) with rehashing support.
 */

#ifndef DICT_H
#define DICT_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <cstring>
#include <functional>
#include <limits.h>

/**
 * @class Dict
 * @brief A dictionary (hash table) implementation with dynamic resizing and rehashing.
 */
class Dict
{
private:
    /**
     * @struct dictEntry
     * @brief Represents an entry in the hash table.
     */
    struct dictEntry
    {
        void *key; ///< Key pointer
        union
        {
            void *val;    ///< Value pointer
            uint64_t u64; ///< Unsigned 64-bit integer
            int64_t s64;  ///< Signed 64-bit integer
            double d;     ///< Double precision floating point
        } v;
        dictEntry *next; ///< Pointer to the next entry (for chaining)
    };

    /**
     * @struct dictht
     * @brief Hash table structure containing entries.
     */
    struct dictht
    {
        dictEntry **table;      ///< Hash table array
        unsigned long size;     ///< Size of the table
        unsigned long sizemask; ///< Mask for indexing
        unsigned long used;     ///< Number of elements used
    };

    /**
     * @struct dict
     * @brief The main dictionary structure with two hash tables (for rehashing).
     */
    struct dict
    {
        dictht ht[2];            ///< Two hash tables used during rehashing
        long rehashidx;          ///< Rehashing index (-1 if not rehashing)
        unsigned long iterators; ///< Number of active iterators
    };

    dict d; ///< Dictionary instance

    // Function pointers for custom hash, duplication, comparison, and destruction
    std::function<unsigned int(const void *)> hashFunction;
    std::function<void *(const void *)> keyDup;
    std::function<void *(const void *)> valDup;
    std::function<int(const void *, const void *)> keyCompare;
    std::function<void(void *)> keyDestructor;
    std::function<void(void *)> valDestructor;

    static const int DICT_HT_INITIAL_SIZE = 4; ///< Initial hash table size
    bool dict_can_resize;                      ///< Global flag for enabling/disabling resizing
    size_t total_size_of_dict = 0;

    /**
     * @brief Checks if resizing is needed.
     * @return True if resizing is needed, false otherwise.
     */
    bool _dictShouldResize()
    {
        // Don't resize if already rehashing
        if (dictIsRehashing(&d))
            return false;

        // Don't resize if the resize flag is disabled, unless it's a forced resize
        float loadFactor = (float)d.ht[0].used / d.ht[0].size;
        if (!dict_can_resize && loadFactor < 5)
            return false;

        // Resize if load factor >= 1 (normal case) or >= 5 (forced resize)
        return loadFactor >= 1;
    }

    /**
     * @brief Performs a single rehashing step.
     */
    void _dictRehashStep()
    {
        if (d.iterators == 0)
            dictRehash(&d, 1);
    }

public:
    /**
     * @brief Constructor for the dictionary.
     * @param hashFunc Hash function.
     * @param keyDupFunc Key duplication function.
     * @param valDupFunc Value duplication function.
     * @param keyCompareFunc Key comparison function.
     * @param keyDestructorFunc Key destructor function.
     * @param valDestructorFunc Value destructor function.
     */
    Dict(std::function<unsigned int(const void *)> hashFunc,
         std::function<void *(const void *)> keyDupFunc,
         std::function<void *(const void *)> valDupFunc,
         std::function<int(const void *, const void *)> keyCompareFunc,
         std::function<void(void *)> keyDestructorFunc,
         std::function<void(void *)> valDestructorFunc)
        : hashFunction(hashFunc), keyDup(keyDupFunc), valDup(valDupFunc),
          keyCompare(keyCompareFunc), keyDestructor(keyDestructorFunc),
          valDestructor(valDestructorFunc)
    {
        _dictInit(&d);
    }

    /**
     * @brief Destructor for the dictionary.
     */
    ~Dict()
    {
        _dictClear(&d);
    }

    /**
     * @brief Enables or disables automatic resizing.
     * @param enable True to enable resizing, false to disable.
     */
    void enableResize(bool enable)
    {
        dict_can_resize = enable;
    }

    /**
     * @brief Adds a key-value pair to the dictionary with automatic rehashing.
     * @param key Key pointer.
     * @param val Value pointer.
     * @return 0 on success, 1 on failure.
     */
    int add(void *key, void *val)
    {
        // Check if resize is needed before adding
        if (_dictShouldResize())
        {
            // Calculate new size based on current usage
            unsigned long newSize = d.ht[0].used * 2;
            _dictExpand(&d, newSize);
        }

        // Perform a rehash step if rehashing is in progress
        if (dictIsRehashing(&d))
            _dictRehashStep();

        return dictAdd(&d, key, val);
    }

    /**
     * @brief Replaces a key's value in the dictionary.
     * @param key Key pointer.
     * @param val Value pointer.
     * @return 0 if key already exists and value is replaced, 1 if key is newly added.
     */
    int replace(void *key, void *val)
    {
        return dictReplace(&d, key, val);
    }

    /**
     * @brief Removes a key from the dictionary with incremental rehashing.
     * @param key Key pointer.
     * @return 0 on success, 1 if key not found.
     */
    int remove(const void *key)
    {
        // Perform a rehash step if rehashing is in progress
        if (dictIsRehashing(&d))
            _dictRehashStep();

        return dictDelete(&d, key);
    }

    /**
     * @brief Finds a key in the dictionary.
     * @param key Key pointer.
     * @return Pointer to the value if found, nullptr otherwise.
     */
    void *find(const void *key)
    {
        // Perform a rehash step if rehashing is in progress
        if (dictIsRehashing(&d))
            _dictRehashStep();

        dictEntry *he = dictFind(&d, key);
        return he ? he->v.val : nullptr;
    }

    /**
     * @brief Performs a rehash operation.
     * @param n Number of steps to rehash.
     * @return 0 on completion, 1 if rehashing is ongoing.
     */
    int rehash(int n)
    {
        return dictRehash(&d, n);
    }

    /**
     * @brief Checks if rehashing is in progress.
     * @return True if rehashing, false otherwise.
     */
    bool isRehashing()
    {
        return dictIsRehashing(&d);
    }

    /**
     * @brief Retrieves the total memory usage of the dictionary for keys, values.
     * @return size_t The total size of the dictionary in bytes.
     */
    size_t get_size_of_dict(){
        return total_size_of_dict;
    }

private:
    void _dictInit(dict *d)
    {
        _dictReset(&d->ht[0]);
        _dictReset(&d->ht[1]);
        d->rehashidx = -1;
        d->iterators = 0;
    }

    void _dictReset(dictht *ht)
    {
        ht->table = nullptr;
        ht->size = 0;
        ht->sizemask = 0;
        ht->used = 0;
    }

    int _dictExpand(dict *d, unsigned long size)
    {
        dictht n;
        unsigned long realsize = _dictNextPower(size);

        if (dictIsRehashing(d) || d->ht[0].used > size)
            return DICT_ERR;

        n.size = realsize;
        n.sizemask = realsize - 1;
        n.table = (dictEntry **)calloc(realsize, sizeof(dictEntry *));
        n.used = 0;

        if (d->ht[0].table == nullptr)
        {
            d->ht[0] = n;
            return DICT_OK;
        }

        d->ht[1] = n;
        d->rehashidx = 0;
        return DICT_OK;
    }

    int _dictExpandIfNeeded(dict *d)
    {
        if (dictIsRehashing(d))
            return DICT_OK;

        if (d->ht[0].size == 0)
            return _dictExpand(d, DICT_HT_INITIAL_SIZE);

        if (d->ht[0].used >= d->ht[0].size)
            return _dictExpand(d, d->ht[0].used * 2);

        return DICT_OK;
    }

    unsigned long _dictNextPower(unsigned long size)
    {
        unsigned long i = DICT_HT_INITIAL_SIZE;
        if (size >= LONG_MAX)
            return LONG_MAX + 1LU;
        while (1)
        {
            if (i >= size)
                return i;
            i *= 2;
        }
    }

    int dictIsRehashing(const dict *d)
    {
        return d->rehashidx != -1;
    }

    void _dictRehashStep(dict *d)
    {
        if (d->iterators == 0)
            dictRehash(d, 1);
    }

    int dictRehash(dict *d, int n)
    {
        int empty_visits = n * 10;
        if (!dictIsRehashing(d))
            return 0;

        while (n-- && d->ht[0].used != 0)
        {
            dictEntry *de, *nextde;

            while (d->ht[0].table[d->rehashidx] == nullptr)
            {
                d->rehashidx++;
                if (--empty_visits == 0)
                    return 1;
            }

            de = d->ht[0].table[d->rehashidx];
            while (de)
            {
                unsigned int h;

                nextde = de->next;
                h = hashFunction(de->key) & d->ht[1].sizemask;
                de->next = d->ht[1].table[h];
                d->ht[1].table[h] = de;
                d->ht[0].used--;
                d->ht[1].used++;
                de = nextde;
            }
            d->ht[0].table[d->rehashidx] = nullptr;
            d->rehashidx++;
        }

        if (d->ht[0].used == 0)
        {
            free(d->ht[0].table);
            d->ht[0] = d->ht[1];
            _dictReset(&d->ht[1]);
            d->rehashidx = -1;
            return 0;
        }

        return 1;
    }

    dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing)
    {
        int index;
        dictEntry *entry;
        dictht *ht;

        if (dictIsRehashing(d))
            _dictRehashStep(d);

        if ((index = _dictKeyIndex(d, key, hashFunction(key), existing)) == -1)
            return nullptr;

        ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
        entry = (dictEntry *)malloc(sizeof(*entry));
        entry->next = ht->table[index];
        ht->table[index] = entry;
        ht->used++;

        entry->key = keyDup ? keyDup(key) : key;
        return entry;
    }

    int dictAdd(dict *d, void *key, void *val)
    {
        dictEntry *entry = dictAddRaw(d, key, nullptr);
        if (!entry)
            return DICT_ERR;
        entry->v.val = valDup ? valDup(val) : val;

        total_size_of_dict += getEntrySize(entry);

        return DICT_OK;
    }

    int dictReplace(dict *d, void *key, void *val)
    {
        dictEntry *entry, *existing;
        entry = dictAddRaw(d, key, &existing);
        if (entry)
        {
            entry->v.val = valDup ? valDup(val) : val;
            return 1;
        }

        if (valDup)
        {
            void *aux = valDup(val);
            if (valDestructor)
                valDestructor(existing->v.val);
            existing->v.val = aux;
        }
        else
        {
            if (valDestructor)
                valDestructor(existing->v.val);
            existing->v.val = val;
        }

        size_t entry_size = getEntrySize(entry);
        size_t existing_entry_size = getEntrySize(entry);
        
        total_size_of_dict += existing_entry_size - existing_entry_size;
        return 0;
    }

    int dictDelete(dict *d, const void *key)
    {
        unsigned int h, idx;
        dictEntry *he, *prevHe;
        int table;

        if (d->ht[0].used == 0 && d->ht[1].used == 0)
            return DICT_ERR;

        if (dictIsRehashing(d))
            _dictRehashStep(d);
        h = hashFunction(key);

        for (table = 0; table <= 1; table++)
        {
            idx = h & d->ht[table].sizemask;
            he = d->ht[table].table[idx];
            prevHe = nullptr;
            while (he)
            {
                if (keyCompare(key, he->key))
                {
                    if (prevHe)
                        prevHe->next = he->next;
                    else
                        d->ht[table].table[idx] = he->next;
                    if (keyDestructor)
                        keyDestructor(he->key);
                    if (valDestructor)
                        valDestructor(he->v.val);
                    total_size_of_dict += getEntrySize(he);
                    free(he);
                    d->ht[table].used--;
                    return DICT_OK;
                }
                prevHe = he;
                he = he->next;
            }
            if (!dictIsRehashing(d))
                break;
        }
        return DICT_ERR;
    }

    dictEntry *dictFind(dict *d, const void *key)
    {
        dictEntry *he;
        unsigned int h, idx, table;

        if (d->ht[0].used + d->ht[1].used == 0)
            return nullptr;
        if (dictIsRehashing(d))
            _dictRehashStep(d);
        h = hashFunction(key);
        for (table = 0; table <= 1; table++)
        {
            idx = h & d->ht[table].sizemask;
            he = d->ht[table].table[idx];
            while (he)
            {
                if (keyCompare(key, he->key))
                    return he;
                he = he->next;
            }
            if (!dictIsRehashing(d))
                return nullptr;
        }
        return nullptr;
    }

    void _dictClear(dict *d)
    {
        _dictClearTable(&d->ht[0]);
        _dictClearTable(&d->ht[1]);
        d->rehashidx = -1;
        d->iterators = 0;
    }

    void _dictClearTable(dictht *ht)
    {
        unsigned long i;

        for (i = 0; i < ht->size && ht->used > 0; i++)
        {
            dictEntry *he, *nextHe;

            if ((he = ht->table[i]) == nullptr)
                continue;
            while (he)
            {
                nextHe = he->next;
                if (keyDestructor)
                    keyDestructor(he->key);
                if (valDestructor)
                    valDestructor(he->v.val);
                free(he);
                ht->used--;
                he = nextHe;
            }
        }
        free(ht->table);
        _dictReset(ht);
    }

    int _dictKeyIndex(dict *d, const void *key, unsigned int hash, dictEntry **existing)
    {
        unsigned int idx, i;
        dictEntry *he;
        if (existing)
            *existing = nullptr;

        if (_dictExpandIfNeeded(d) == DICT_ERR)
            return -1;
        for (i = 0; i <= 1; i++)
        {
            idx = hash & d->ht[i].sizemask;
            he = d->ht[i].table[idx];
            while (he)
            {
                if (keyCompare(key, he->key))
                {
                    if (existing)
                        *existing = he;
                    return -1;
                }
                he = he->next;
            }
            if (!dictIsRehashing(d))
                break;
        }
        return idx;
    }

    size_t getEntrySize(dictEntry *entry)
    {
        if(!entry) return 0;

        size_t entry_size = sizeof(dictEntry); // Base struct size

        // Add key size (assuming key is a string)
        if (entry->key)
            entry_size += strlen(static_cast<char *>(entry->key)) + 1; // +1 for null terminator

        // Add value size (assuming value is a string or dynamically allocated object)
        if (entry->v.val)
            entry_size += strlen(static_cast<char *>(entry->v.val)) + 1;

        return entry_size;
    }

    static const int DICT_OK = 0;
    static const int DICT_ERR = 1;
};


/**
 * @brief Hash function for C-style strings.
 * 
 * This function computes a hash value for a given string using the 
 * DJB2 algorithm. It iterates through the characters and accumulates 
 * a hash value.
 * 
 * @param key Pointer to the C-style string key.
 * @return unsigned int The computed hash value.
 */
unsigned int stringHash(const void* key) {
    const char* str = static_cast<const char*>(key);
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}

/**
 * @brief Compares two C-style string keys.
 * 
 * This function compares two string keys using `strcmp` and returns 
 * whether they are equal.
 * 
 * @param key1 Pointer to the first string key.
 * @param key2 Pointer to the second string key.
 * @return int Returns 1 if keys are equal, 0 otherwise.
 */
int stringCompare(const void* key1, const void* key2) {
    return strcmp(static_cast<const char*>(key1), static_cast<const char*>(key2)) == 0;
}

/**
 * @brief Duplicates a C-style string key.
 * 
 * This function creates a copy of the given string key using `strdup`.
 * The caller is responsible for freeing the allocated memory.
 * 
 * @param key Pointer to the original string key.
 * @return void* Pointer to the duplicated string.
 */
void* stringDup(const void* key) {
    return strdup(static_cast<const char*>(key));
}

/**
 * @brief Frees a dynamically allocated C-style string.
 * 
 * This function releases the memory allocated for a string key or value.
 * 
 * @param ptr Pointer to the string to be freed.
 */
void freeString(void* ptr) {
    free(ptr);
}

#endif // DICT_H