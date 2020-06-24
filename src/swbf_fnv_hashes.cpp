
#include "swbf_fnv_hashes.hpp"
#include "synced_cout.hpp"
#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

using namespace std::literals;

std::unordered_map<std::uint32_t, std::string> swbf_hashes;


bool dictionary_is_initialized = false;


std::string lookup_fnv_hash(const std::uint32_t hash)
{
   if (!dictionary_is_initialized) {
      dictionary_is_initialized = true;
      read_dictionary();
   }
   const auto result = swbf_hashes.find(hash);

   if (result != std::cend(swbf_hashes)) return std::string{result->second};

   // here hex is nice for 2 reasons: 
   //   1. easier to see in hex editor  
   //   2. it matches the output of the ToolsFl\bin\hash.exe program
   char hex_string[20];
   sprintf(hex_string, "0x%x", hash);
   synced_cout::print("Warning: Unknown hash looked up.\n"s, "   value: "s, hex_string, '\n');

   return std::string(hex_string);
}

// Reads in the dictionary of the 'known' strings we'll be trying to find the hashes for
void read_dictionary()
{
   uint32_t hash = 0;
   std::ifstream input("dictionary.txt");
   for (std::string line; getline(input, line);) {
      hash = fnv_1a_hash(line);
      if (swbf_hashes.find(hash) == std::cend(swbf_hashes)) {
         swbf_hashes[hash] = line;
      }
   }
   input.close();
}