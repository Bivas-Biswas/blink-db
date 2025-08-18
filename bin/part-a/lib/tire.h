/**
 * @file trie.h
 * @brief Trie data structure for key storage with file offsets.
 * 
 * This Trie implementation supports insert, search, remove, and deletion status checking.
 */

 #ifndef TRIE_H
 #define TRIE_H
 
 #include <iostream>
 #include <string>
 #include <unordered_map>
 
 /**
  * @class TrieNode
  * @brief Represents a node in the Trie.
  */
 class TrieNode
 {
 public:
     std::unordered_map<char, TrieNode *> children; /**< Child nodes of the Trie. */
     long file_offset = -1; /**< Offset of the file where the key is stored. */
     bool isDeleted = false; /**< Flag indicating whether the key is deleted. */
 };
 
 /**
  * @class Trie
  * @brief Trie data structure for efficient key lookup.
  */
 class Trie
 {
 private:
     TrieNode *root; /**< Root node of the Trie. */
 
 public:
     /**
      * @brief Constructs a new Trie object.
      */
     Trie() { root = new TrieNode(); }
     
     /**
      * @brief Destroys the Trie object and deallocates memory.
      */
     ~Trie() { deleteTrie(root); }
 
     /**
      * @brief Inserts a key with a file offset into the Trie.
      * @param key The key to insert.
      * @param offset The file offset associated with the key.
      */
     void insert(const std::string &key, long offset)
     {
         TrieNode *node = root;
         for (char ch : key)
         {
             if (!node->children.count(ch))
                 node->children[ch] = new TrieNode();
             node = node->children[ch];
         }
         node->file_offset = offset;
         node->isDeleted = false;
     }
 
     /**
      * @brief Searches for a key in the Trie.
      * @param key The key to search for.
      * @return The file offset if found, otherwise -1.
      */
     long search(const std::string &key)
     {
         TrieNode *node = root;
         for (char ch : key)
         {
             if (!node->children.count(ch))
                 return -1;
             node = node->children[ch];
         }
         return node->isDeleted ? -1 : node->file_offset;
     }
 
     /**
      * @brief Marks a key as deleted in the Trie.
      * @param key The key to remove.
      */
     void remove(const std::string &key)
     {
         TrieNode *node = root;
         for (char ch : key)
         {
             if (!node->children.count(ch))
                 return;
             node = node->children[ch];
         }
         node->isDeleted = true;
     }
 
     /**
      * @brief Checks if a key is marked as deleted.
      * @param key The key to check.
      * @return True if the key is deleted, false otherwise.
      */
     bool isDeleted(const std::string &key)
     {
         TrieNode *node = root;
         for (char ch : key)
         {
             if (!node->children.count(ch))
                 return false;
             node = node->children[ch];
         }
         return node->isDeleted;
     }
 
 private:
     /**
      * @brief Recursively deletes all nodes in the Trie.
      * @param node The current Trie node.
      */
     void deleteTrie(TrieNode *node)
     {
         for (auto &[ch, child] : node->children)
         {
             deleteTrie(child);
         }
         delete node;
     }
 };
 
 #endif
 