#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

#include <iostream>
#include <vector>

/**
 * @class BloomFilter
 * @brief Implements a simple Bloom filter for fast key existence checks.
 */
class BloomFilter
{
private:
    std::vector<int> filter; ///< Dynamic bitset for the Bloom filter.
    int filter_size;       ///< Size of the Bloom filter.

public:
    /**
     * @brief Constructor to initialize the Bloom filter with a given size.
     * @param _size The size of the Bloom filter (default: 10,000).
     */
    explicit BloomFilter(int _size = 10000);

    /**
     * @brief Inserts a key into the Bloom filter.
     * @param _key The key to insert.
     */
    void insert(const std::string &_key);

    /**
     * @brief Checks if a key exists in the Bloom filter.
     * @param _key The key to check.
     * @return True if the key is possibly in the filter, false otherwise.
     */
    bool contains(const std::string &_key);

    /**
     * @brief Removes a key from the Bloom filter (Note: Bloom filters generally do not support removals correctly).
     * @param _key The key to remove.
     */
    void remove(const std::string &_key);

private:
    /**
     * @brief Hashes a key to obtain a bit position in the Bloom filter.
     * @param _key The key to hash.
     * @return The hashed index within the bitset.
     */
    size_t hashKey(const std::string &_key);
};

BloomFilter::BloomFilter(int _size) : filter(_size, false), filter_size(_size) {}

void BloomFilter::insert(const std::string &_key)
{
    filter[hashKey(_key)] += 1;
}

bool BloomFilter::contains(const std::string &_key)
{
    return filter[hashKey(_key)] > 0;
}

void BloomFilter::remove(const std::string &_key)
{
    if(filter[hashKey(_key)] == 0) return;
    filter[hashKey(_key)] -= 1;
}

size_t BloomFilter::hashKey(const std::string &_key)
{
    std::hash<std::string> hashFn1;
    std::hash<std::size_t> hashFn2;
    return hashFn2(hashFn1(_key)) % filter_size;
}

#endif // BLOOMFILTER_H
