#include "layer_index.hpp"
#include "file_saver.hpp"

#include <fmt/format.h>

void Layer_index::add(std::string_view world, Layer layer)
{
   std::scoped_lock lock{_mutex};

   _index[std::string{world}].push_back(std::move(layer));
}

void Layer_index::save(File_saver& saver)
{
   std::scoped_lock lock{_mutex};

   for (auto& [name, layers] : _index) {
      std::sort(layers.begin(), layers.end(),
                [](const Layer& l, const Layer& r) { return l.index < r.index; });

      std::string buffer;
      buffer.reserve(layers.size() * 256);

      buffer += "Version(1);\n";
      buffer += "NextID(1);\n";

      for (const auto& [layer_name, i] : layers) {
         buffer += fmt::format("Layer(\"{}\", {}, 0)\n", layer_name, i);
         buffer += "{\n";
         buffer += "   Description(\"\");\n";
         buffer += "}\n\n";
      }

      saver.save_file(buffer, "world", name, ".LDX");
   }
}