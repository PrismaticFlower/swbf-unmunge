#pragma once

class File_saver;

#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Layer_index {
public:
   struct Layer {
      std::string name;
      int index = 0;
   };

   void add(std::string_view world, Layer layer);

   void save(File_saver& saver);

private:
   std::shared_mutex _mutex;
   std::unordered_map<std::string, std::vector<Layer>> _index;
};
