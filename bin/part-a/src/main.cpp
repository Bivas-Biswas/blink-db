/**
 * @file main.cpp
 * @brief Command-line interface for interacting with the LRU cache database.
 *
 * This program provides a simple CLI to interact with an LRUCache-based key-value store.
 * Supported commands:
 * - SET <key> <value>: Stores the key-value pair.
 * - GET <key>: Retrieves the value for a given key.
 * - DEL <key>: Deletes a key from the cache.
 * - EXIT: Terminates the program.
 */

 #include <iostream>
 #include <string>
 #include <filesystem>
 #include <sstream>
 
 #include "../lib/lru_cache.h"
 
 /**
  * @brief Main function providing a command-line interface for the LRU cache.
  * 
  * @return int Program exit status.
  */
 int main()
 {
     LRUCache database;
     std::cout << "Enter command (SET key value, GET key, DEL key, or EXIT to quit):" << std::endl;
 
     std::string input, command, key, value;
     while (true)
     {
         std::cout << "> ";
         std::getline(std::cin, input);
 
         std::istringstream iss(input);
         iss >> command;
         if (command == "SET")
         {
             iss >> key >> value;
             if (key.empty() || value.empty())
             {
                 std::cout << "Invalid SET command. Usage: SET <key> <value>" << std::endl;
                 continue;
             }
             database.set(strdup(key.c_str()), strdup(value.c_str()));
         }
         else if (command == "GET")
         {
             iss >> key;
             if (key.empty())
             {
                 std::cout << "Invalid GET command. Usage: GET <key>" << std::endl;
                 continue;
             }
             std::string value = database.get(key.c_str());
             std::cout << (value == "-1" ? "NULL" : value) << std::endl;
         }
         else if (command == "DEL")
         {
             iss >> key;
             if (key.empty())
             {
                 std::cout << "Invalid DEL command. Usage: DEL <key>" << std::endl;
                 continue;
             }
             int status = database.del(key.c_str());
             if (status)
             {
                 std::cout << "Does not exist" << std::endl;
             }
         }
         else if (command == "EXIT")
         {
             break;
         }
         else
         {
             std::cout << "Unknown command. Use SET, GET, DEL, or EXIT." << std::endl;
         }
     }
 
     return 0;
 }
 