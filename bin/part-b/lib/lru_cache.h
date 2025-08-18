#ifndef LRU_CACHE_BLINK_H
#define LRU_CACHE_BLINK_H

#include <iostream>

#include "./dict.h"
#include "./persistence_kv_store.h"

/**
 * @class LRUCache
 * @brief Implements a Least Recently Used (LRU) cache with memory constraints
 *
 * This class provides a memory-constrained LRU cache implementation that evicts
 * least recently used items when memory limits are exceeded. It uses a doubly linked
 * list for tracking usage order and a dictionary for O(1) lookups.
 */
class LRUCache
{
public:
    /**
     * @struct Node
     * @brief Represents a node in the doubly linked list for the LRU cache
     *
     * Each node contains a key-value pair and pointers to the next and previous nodes.
     */
    struct Node
    {
        void *key;   /**< Pointer to the key */
        void *value; /**< Pointer to the value */
        Node *next;  /**< Pointer to the next node in the list */
        Node *prev;  /**< Pointer to the previous node in the list */

        /**
         * @brief Constructs a new Node with the given key and value
         *
         * @param k Pointer to the key
         * @param v Pointer to the value
         */
        Node(void *k, void *v)
        {
            key = k;
            value = v;
            next = nullptr;
            prev = nullptr;
        }
    };

    Dict dict;                   /**< Dictionary for O(1) lookups */
    Node *head;                  /**< Head of the doubly linked list */
    Node *tail;                  /**< Tail of the doubly linked list */
    size_t current_memory_usage; /**< Current memory usage in bytes */
    size_t max_memory_bytes;     /**< Maximum memory limit in bytes */
    PersistenceKVStore storage;
    std::string value;

    /**
     * @brief Constructs a new LRU Cache with the specified memory limit
     *
     * @param max_mem Maximum memory limit in bytes
     */
    LRUCache(size_t max_mem = 1024 * 1024 * 1024) : storage("./blink"), dict(stringHash, nullptr, nullptr, stringCompare, freeKey, freeValue), current_memory_usage(0)
    {
        max_memory_bytes = max_mem;
        head = new Node(strdup("-1"), strdup("-1"));
        tail = new Node(strdup("-1"), strdup("-1"));
        head->next = tail;
        tail->prev = head;
    }

    /**
     * @brief Destroys the LRU Cache and frees all allocated memory
     */
    ~LRUCache()
    {
        freeNode(head);
        freeNode(tail);
    }

    /**
     * @brief Retrieves the value for a given key
     *
     * If the key exists, it moves the corresponding node to the front of the list
     * to mark it as most recently used.
     *
     * @param key The key to look up
     * @return std::string The value associated with the key, or "-1" if not found
     */
    std::string get(const void *key)
    {   
        Node *retrievedValue = static_cast<Node *>(dict.find(key));
        if (!retrievedValue){
            std::string value;
            if(storage.get(std::string((char *)key), value)){
                void *key1 = (void *)key;
                void* value1 = (void *) strdup(value.c_str());
                Node *node = new Node(key1, value1);
                dict.add(key1, node);
                add(node);
                current_memory_usage += getSize((char *)key) + getNodeSize(node);
                return value;
            };
            return "-1";
        }
        

        remove(retrievedValue);
        add(retrievedValue);
        return static_cast<char *>(retrievedValue->value);
    }

    /**
     * @brief Prints the current state of the cache for debugging
     */
    void printList()
    {
        Node *curr = head->next;
        std::cout << "Cache state: ";
        while (curr != tail)
        {
            std::cout << "[" << static_cast<char *>(curr->key) << ":" << static_cast<char *>(curr->value) << "] ";
            curr = curr->next;
        }
        std::cout << std::endl;
    }

    /**
     * @brief Adds or updates a key-value pair in the cache
     *
     * If adding the new item exceeds the memory capacity, the least recently used
     * items will be evicted until the memory usage is within limits.
     *
     * @param key Pointer to the key
     * @param value Pointer to the value
     */
    void set(void *key, void *value)
    {
        Node *retrievedValue = static_cast<Node *>(dict.find(key));
        Node *node = new Node(key, value);

        if (retrievedValue)
        {
            current_memory_usage += getNodeSize(node) - getNodeSize(retrievedValue);
            remove(retrievedValue);
            dict.replace(key, node);
            add(node);
        }
        else
        {
            dict.add(key, node);
            add(node);
            current_memory_usage += getSize((char *)key) + getNodeSize(node);
        }

        if (current_memory_usage >= max_memory_bytes)
        {
            Node *nodeToDelete = tail->prev;
            storage.insert(std::string(strdup((char *)nodeToDelete->key)), std::string(strdup((char *)nodeToDelete->value)));
            remove(nodeToDelete);
            std::cout << current_memory_usage << std::endl;
            current_memory_usage -= getSize((char *)nodeToDelete->key) + getNodeSize(nodeToDelete);
            dict.remove(nodeToDelete->key);
            std::cout << "Eviction happen" << std::endl;
            std::cout << current_memory_usage << std::endl;
        }
    }

    /**
     * @brief Deletes a key-value pair from the cache
     *
     * @param key The key to delete
     * @return int 0 if successful, 1 if the key was not found
     */
    int del(const void *key)
    {
        Node *retrievedValue = static_cast<Node *>(dict.find(key));

        if (retrievedValue)
        {
            remove(retrievedValue);
            current_memory_usage -= getSize((char *)retrievedValue->key) + getNodeSize(retrievedValue);
            dict.remove(retrievedValue->key);
            return 0;
        }
        return 1;
    }

    /**
     * @brief Gets the current memory usage of the cache
     *
     * @return size_t Current memory usage in bytes
     */
    size_t memory_usage()
    {
        return current_memory_usage;
    }

    /**
     * @brief Gets the maximum memory limit of the cache
     *
     * @return size_t Maximum memory limit in bytes
     */
    size_t max_memory()
    {
        return max_memory_bytes;
    }

    /**
     * @brief Gets the number of items in the cache
     *
     * @return size_t Number of items in the cache
     */
    size_t size()
    {
        return dict.size();
    }

private:
    /**
     * @brief Frees memory allocated for a node
     *
     * @param ptr Pointer to the node to free
     */
    void freeNode(void *ptr)
    {
        if (!ptr)
            return; // Safety check
        Node *node = static_cast<Node *>(ptr);

        freeKey(node->key);
        freeValue(node);
    }

    /**
     * @brief Gets the size of a string
     *
     * @param ptr Pointer to the string
     * @return size_t Length of the string
     */
    size_t getSize(char *ptr)
    {
        if (!ptr)
            return 0;
        return strlen(ptr);
    }

    /**
     * @brief Gets the combined size of a node's key and value
     *
     * @param ptr Pointer to the node
     * @return size_t Combined size of the node's key and value
     */
    size_t getNodeSize(void *ptr)
    {
        Node *node = (Node *)ptr;
        return getSize((char *)node->key) + getSize((char *)node->value);
    }

    /**
     * @brief Static function to free a key
     *
     * @param ptr Pointer to the key to free
     */
    static void freeKey(void *ptr)
    {
        free(ptr);
    }

    /**
     * @brief Static function to free a value (node)
     *
     * @param ptr Pointer to the node to free
     */
    static void freeValue(void *ptr)
    {
        Node *node = (Node *)ptr;
        free(node->value);
        free(node);
    }

    /**
     * @brief Adds a node right after the head (most recently used position)
     *
     * @param node Pointer to the node to add
     */
    void add(Node *node)
    {
        if (!node)
            return;

        Node *nextNode = head->next;
        head->next = node;
        node->prev = head;
        node->next = nextNode;
        nextNode->prev = node;
    }

    /**
     * @brief Removes a node from the list
     *
     * @param node Pointer to the node to remove
     */
    void remove(Node *node)
    {
        if (!node)
            return;

        Node *prevNode = node->prev;
        Node *nextNode = node->next;
        prevNode->next = nextNode;
        nextNode->prev = prevNode;
    }
};

#endif