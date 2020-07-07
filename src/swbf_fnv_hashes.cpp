
#include "swbf_fnv_hashes.hpp"
#include "synced_cout.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

using namespace std::literals;

std::unordered_map<std::uint32_t, std::string> swbf_hashes;

bool dictionary_is_initialized = false;

BOOL GetModuleDir(char* pBuf, DWORD dwBufSize)
{
   BOOL retVal = FALSE;
   TCHAR tBuf[255];
   if (GetModuleFileName(NULL, tBuf, dwBufSize)) {
      PathRemoveFileSpec(tBuf); // remove executable name
      CharToOem(tBuf, pBuf);
      strcat(pBuf, "\\"); // add the path sep
      retVal =  TRUE;
   }
   return retVal;
}

std::string lookup_fnv_hash(const std::uint32_t hash)
{
   if (!dictionary_is_initialized) {
      dictionary_is_initialized = true;

      char buf[270]; // enough space for 255 path characters + 'dictionary.txt';
      if (GetModuleDir(buf, 255)) {
		 strcat(buf, "dictionary.txt");
         read_dictionary(buf);
      }
      else {
         std::cout << "Error! not enough buffer to reference the dictionary. \n" <<
			 "Place this program closer to the Drive root\n" <<
			 "(hint: shortest possible path would be 'C:\\swbf-unmunge\\').\n";
      }

	  // if they left a 'tmp_dict' in the exe dir, read it.
	  if (GetModuleDir(buf, 255)) {
         strcat(buf, "tmp_dict.txt");
         read_dictionary(buf);
      }

	  // let's also be nice and read the one from the current directory.
      read_dictionary("tmp_dict.txt");
   }
   const auto result = swbf_hashes.find(hash);

   if (result != std::cend(swbf_hashes)) return std::string{result->second};

   // here hex is nice for 2 reasons:
   //   1. easier to see in hex editor
   //   2. it matches the output of the ToolsFl\bin\hash.exe program
   char hex_string[20];
   sprintf(hex_string, "0x%x", hash);
   synced_cout::print("Warning: Unknown hash looked up.\n"s, "   value: "s, hex_string,
                      '\n');

   return std::string(hex_string);
}

// Reads in the dictionary of the 'known' strings we'll be trying to find the hashes for
void read_dictionary(const char* fileName)
{
   uint32_t hash = 0;
   std::ifstream input(fileName);
   for (std::string line; getline(input, line);) {
      hash = fnv_1a_hash(line);
      if (swbf_hashes.find(hash) == std::cend(swbf_hashes)) {
         swbf_hashes[hash] = line;
      }
   }
   input.close();
}