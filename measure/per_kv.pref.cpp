#include <iostream>

#include "../lib/persistence_kv_store.h"

int main()
{
    PersistenceKVStore store("store1");

    store.insert("key2", "value1");
    store.insert("key2", "value2");
    store.insert("key1", "value2");

    std::string value;
    if (store.get("key1", value))
    {
        std::cout << "Retrieved: " << value << std::endl;
    }
    else
    {
        std::cout << "Key not found" << std::endl;
    }

    store.remove("key1");
    if (store.get("key1", value))
    {
        std::cout << "Retrieved: " << value << std::endl;
    }
    else
    {
        std::cout << "Key not found after deletion" << std::endl;
    }
    while (true);
    return 0;
}