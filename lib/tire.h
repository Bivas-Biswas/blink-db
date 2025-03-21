#ifndef TRIE_H
#define TRIE_H

#include <iostream>
#include <string>
#include <unordered_map>

class TrieNode
{
public:
    std::unordered_map<char, TrieNode *> children;
    long file_offset = -1;
    bool isDeleted = false;
};

class Trie
{
private:
    TrieNode *root;

public:
    Trie() { root = new TrieNode(); }
    ~Trie() { deleteTrie(root); }

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