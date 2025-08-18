#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

const int ORDER = 4;
const std::string BPTREE_FILE = "bptree.dat";

struct BPTreeNode {
    bool isLeaf;
    int numKeys;
    std::string keys[ORDER - 1];
    uint64_t values[ORDER - 1];  // For leaf nodes
    long children[ORDER];        // File offsets for child nodes

    BPTreeNode(bool leaf = false) {
        isLeaf = leaf;
        numKeys = 0;
        std::memset(values, 0, sizeof(values));
        std::memset(children, -1, sizeof(children));
    }

    void serialize(std::ofstream &out) const {
        out.write(reinterpret_cast<const char *>(&isLeaf), sizeof(isLeaf));
        out.write(reinterpret_cast<const char *>(&numKeys), sizeof(numKeys));

        for (int i = 0; i < ORDER - 1; i++) {
            size_t len = keys[i].size();
            out.write(reinterpret_cast<const char *>(&len), sizeof(len));
            out.write(keys[i].c_str(), len);
        }

        out.write(reinterpret_cast<const char *>(values), sizeof(values));
        out.write(reinterpret_cast<const char *>(children), sizeof(children));
    }

    void deserialize(std::fstream &in) {
        in.read(reinterpret_cast<char *>(&isLeaf), sizeof(isLeaf));
        in.read(reinterpret_cast<char *>(&numKeys), sizeof(numKeys));

        for (int i = 0; i < ORDER - 1; i++) {
            size_t len;
            in.read(reinterpret_cast<char *>(&len), sizeof(len));
            keys[i].resize(len);
            in.read(&keys[i][0], len);
        }

        in.read(reinterpret_cast<char *>(values), sizeof(values));
        in.read(reinterpret_cast<char *>(children), sizeof(children));
    }
};

class BPTree {
private:
    std::fstream file;
    long rootOffset;

    long writeNode(const BPTreeNode &node) {
        file.seekp(0, std::ios::end);
        long offset = file.tellp();
        node.serialize(file);
        file.flush();
        return offset;
    }

    void readNode(long offset, BPTreeNode &node) {
        file.seekg(offset);
        node.deserialize(file);
    }

public:
    BPTree() {
        file.open(BPTREE_FILE, std::ios::in | std::ios::out | std::ios::binary);
        if (!file) {
            file.open(BPTREE_FILE, std::ios::out | std::ios::binary);
            BPTreeNode root(true);
            rootOffset = writeNode(root);
        } else {
            rootOffset = 0;
        }
    }

    ~BPTree() {
        file.close();
    }

    void insert(const std::string &key, uint64_t offset) {
        BPTreeNode root;
        readNode(rootOffset, root);

        if (root.numKeys == ORDER - 1) {
            BPTreeNode newRoot(false);
            newRoot.children[0] = rootOffset;
            splitChild(newRoot, 0, root);
            rootOffset = writeNode(newRoot);
        }
        insertNonFull(rootOffset, key, offset);
    }

    void remove(const std::string &key) {
        deleteKey(rootOffset, key);
    }

    uint64_t search(const std::string &key) {
        return searchKey(rootOffset, key);
    }

private:
    void splitChild(BPTreeNode &parent, int index, BPTreeNode &child) {
        BPTreeNode newChild(child.isLeaf);
        newChild.numKeys = (ORDER - 1) / 2;

        for (int i = 0; i < newChild.numKeys; i++)
            newChild.keys[i] = child.keys[i + ORDER / 2];

        if (!child.isLeaf) {
            for (int i = 0; i <= newChild.numKeys; i++)
                newChild.children[i] = child.children[i + ORDER / 2];
        } else {
            for (int i = 0; i < newChild.numKeys; i++)
                newChild.values[i] = child.values[i + ORDER / 2];
        }

        child.numKeys = (ORDER - 1) / 2;
        long newChildOffset = writeNode(newChild);

        for (int i = parent.numKeys; i > index; i--) {
            parent.keys[i] = parent.keys[i - 1];
            parent.children[i + 1] = parent.children[i];
        }

        parent.keys[index] = child.keys[ORDER / 2 - 1];
        parent.children[index + 1] = newChildOffset;
        parent.numKeys++;
        writeNode(parent);
        writeNode(child);
    }

    void insertNonFull(long offset, const std::string &key, uint64_t value) {
        BPTreeNode node;
        readNode(offset, node);

        int i = node.numKeys - 1;
        if (node.isLeaf) {
            while (i >= 0 && key < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                node.values[i + 1] = node.values[i];
                i--;
            }
            node.keys[i + 1] = key;
            node.values[i + 1] = value;
            node.numKeys++;
            writeNode(node);
        } else {
            while (i >= 0 && key < node.keys[i])
                i--;
            i++;

            BPTreeNode child;
            readNode(node.children[i], child);
            if (child.numKeys == ORDER - 1) {
                splitChild(node, i, child);
                if (key > node.keys[i]) i++;
            }
            insertNonFull(node.children[i], key, value);
        }
    }

    void deleteKey(long offset, const std::string &key) {
        BPTreeNode node;
        readNode(offset, node);

        int i = 0;
        while (i < node.numKeys && key > node.keys[i]) i++;

        if (i < node.numKeys && key == node.keys[i]) {
            if (node.isLeaf) {
                for (int j = i; j < node.numKeys - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                    node.values[j] = node.values[j + 1];
                }
                node.numKeys--;
                writeNode(node);
            } else {
                BPTreeNode child;
                readNode(node.children[i + 1], child);
                node.keys[i] = child.keys[0];
                node.values[i] = child.values[0];
                deleteKey(node.children[i + 1], child.keys[0]);
            }
        } else if (!node.isLeaf) {
            deleteKey(node.children[i], key);
        }
    }

    uint64_t searchKey(long offset, const std::string &key) {
        BPTreeNode node;
        readNode(offset, node);

        int i = 0;
        while (i < node.numKeys && key > node.keys[i]) i++;

        if (i < node.numKeys && key == node.keys[i])
            return node.values[i];

        if (node.isLeaf)
            return -1;

        return searchKey(node.children[i], key);
    }
};

// Example Usage
int main() {
    BPTree bpt;

    bpt.insert("apple", 100);
    bpt.insert("banana", 200);
    bpt.insert("cherry", 300);
    bpt.insert("date", 400);

    std::cout << "Offset of banana: " << bpt.search("banana") << std::endl;
    bpt.remove("banana");
    std::cout << "Offset of banana after delete: " << bpt.search("banana") << std::endl;

    return 0;
}
