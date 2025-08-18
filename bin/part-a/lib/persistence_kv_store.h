#ifndef PERSISTENCE_KV_STORE
#define PERSISTENCE_KV_STORE

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <bitset>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

#include "./bloomfilter.h"
#include "./tire.h"

/**
 * @class PersistenceKVStore
 * @brief A persistent key-value store with background rewriting and indexing.
 */
class PersistenceKVStore
{
private:
    Trie *index;                   ///< Trie-based index for fast lookups.
    BloomFilter bloomFilter;       ///< Bloom filter for quick key existence checks.
    std::fstream dataFile;         ///< File stream for data storage.
    std::string filename;          ///< Name of the main database file.
    std::string tempfilename;      ///< Temporary file for rewriting.
    std::thread rewriteThread;     ///< Background thread for rewrite operations.
    std::atomic<bool> stopRewrite; ///< Flag to stop background rewriting.
    int rewrite_interval_ms;       ///< Interval for rewrite scheduling.
    std::mutex mtx_index;

public:
    /**
     * @brief Constructor: Initializes the key-value store, loads the index, and starts the rewrite scheduler.
     * @param _dbname The database name.
     * @param _bloom_filter_size The size of BloomFilter.
     * @param _rewrite_interval Interval for background rewrite in milliseconds (default: 5000).
     */
    PersistenceKVStore(const std::string _dbname, int _bloom_filter_size = 10000, int rewrite_interval = 5000);

    /**
     * @brief Destructor: Ensures background rewriting stops and closes the file.
     */
    ~PersistenceKVStore();

    /**
     * @brief Inserts a key-value pair into the store.
     * @param _key The key to insert.
     * @param _value The corresponding value.
     */
    void insert(const std::string &_key, const std::string &_value);

    /**
     * @brief Retrieves the value for a given key.
     * @param _key The key to search for.
     * @param _value Reference to store the retrieved value.
     * @return True if the key exists, false otherwise.
     */
    bool get(const std::string &_key, std::string &_value);

    /**
     * @brief Removes a key from the store.
     * @param _key The key to remove.
     */
    void remove(const std::string &_key);

    /**
     * @brief Removes the database file.
     */
    void remove_db();

private:
    /**
     * @brief Synchronizes the in-memory index with the stored file.
     */
    void syncIndex();

    /**
     * @brief Starts a periodic rewrite scheduler to clean up deleted keys.
     */
    void startRewriteScheduler();

    /**
     * @brief Triggers a rewrite operation to remove deleted entries.
     */
    void triggerRewrite();
};

PersistenceKVStore::PersistenceKVStore(const std::string _dbname, int _bloom_filter_size, int _rewrite_interval)
    : bloomFilter(_bloom_filter_size)
{
    filename = _dbname + ".txt";
    tempfilename = _dbname + ".temp.txt";
    rewrite_interval_ms = _rewrite_interval;
    stopRewrite.store(false);
    dataFile.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    index = new Trie();

    if (!dataFile)
    {
        dataFile.open(filename, std::ios::out | std::ios::binary);
        dataFile.close();
        dataFile.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    }

    syncIndex();
    rewriteThread = std::thread(&PersistenceKVStore::startRewriteScheduler, this);
}

PersistenceKVStore::~PersistenceKVStore()
{
    dataFile.clear();
    stopRewrite.store(true);
    if (rewriteThread.joinable())
    {
        rewriteThread.join();
    }
    dataFile.close();
}

void PersistenceKVStore::insert(const std::string &_key, const std::string &_value)
{
    dataFile.clear();
    dataFile.seekp(0, std::ios::end);
    long offset = dataFile.tellp();
    if (offset == -1)
    {
        std::cerr << "Error: tellp() returned -1" << std::endl;
        return;
    }

    dataFile << _key << " " << _value << std::endl;
    dataFile.flush();
    std::lock_guard<std::mutex> lock(mtx_index);
    index->insert(_key, offset);
    bloomFilter.insert(_key);
}

bool PersistenceKVStore::get(const std::string &_key, std::string &_value)
{
    std::lock_guard<std::mutex> lock(mtx_index);
    if (!bloomFilter.contains(_key))
        return false;

    long offset = index->search(_key);
    if (offset == -1)
        return false;

    dataFile.clear();
    dataFile.seekg(offset, std::ios::beg);

    if (!dataFile)
        return false;

    std::string storedKey;
    dataFile >> storedKey;

    if (storedKey != _key)
        return false;

    std::getline(dataFile >> std::ws, _value);

    return !_value.empty();
}

void PersistenceKVStore::remove(const std::string &_key)
{
    std::lock_guard<std::mutex> lock(mtx_index);
    index->remove(_key);
    bloomFilter.remove(_key);
}

void PersistenceKVStore::remove_db()
{
    std::remove(filename.c_str());
}

void PersistenceKVStore::syncIndex()
{
    dataFile.clear();
    dataFile.seekg(0, std::ios::end);

    if (dataFile.tellg() == 0)
    {
        return;
    }

    dataFile.seekg(0, std::ios::beg);

    std::string key, value;
    while (true)
    {
        long offset = dataFile.tellg();

        if (!(dataFile >> key >> value))
        {
            break;
        }

        index->insert(key, offset);
        bloomFilter.insert(key);
    }
}

void PersistenceKVStore::startRewriteScheduler()
{
    while (!stopRewrite.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(rewrite_interval_ms));
        triggerRewrite();
    }
}

void PersistenceKVStore::triggerRewrite()
{
    std::ofstream tempFile(tempfilename, std::ios::trunc);
    dataFile.clear();
    dataFile.seekg(0, std::ios::beg);

    Trie *new_index = new Trie();

    std::string key, value;

    while (true)
    {
        long curr_pos = dataFile.tellg();

        if (!(dataFile >> key >> value))
        {
            break;
        }

        auto key_offset = index->search(key);

        if (key_offset == -1)
        {
            continue;
        }

        if (key_offset == curr_pos + 1)
        {
            long offset = tempFile.tellp();
            tempFile << key << " " << value << std::endl;
            new_index->insert(key, offset);
        }
    }

    tempFile.close();

    std::ofstream newDataFile(filename, std::ios::trunc);
    std::ifstream tempData(tempfilename);
    newDataFile << tempData.rdbuf();
    tempData.close();
    newDataFile.close();
    std::remove(tempfilename.c_str());

    std::lock_guard<std::mutex> lock(mtx_index);

    Trie *old_index = index;
    index = new_index;
    delete old_index;

    dataFile.open(filename, std::ios::in | std::ios::out | std::ios::binary);
}

#endif
