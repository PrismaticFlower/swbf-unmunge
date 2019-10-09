
#include "file_saver.hpp"
#include "string_helpers.hpp"
#include "ucfb_builder.hpp"

#include "tbb/concurrent_vector.h"
#include "tbb/task_group.h"

#include <string>

namespace fs = std::filesystem;

using namespace std::literals;

namespace {

Ucfb_builder assemble_directory(const fs::path& directory);

struct Directory_info {
   std::size_t index{};
   Magic_number magic_number;
};

Directory_info decompose_name(std::string_view string)
{
   const auto components = split_string(string, ' ');

   Directory_info info;
   info.index = std::stoull(std::string{components[0]});

   if (components[1].length() == 4) {
      info.magic_number = create_magic_number(components[1][0], components[1][1],
                                              components[1][2], components[1][3]);
   }
   else {
      info.magic_number = deserialize_magic_number(components[1]);
   }

   return info;
}

Ucfb_builder read_saved_chunk(const fs::path& file_path, const Magic_number magic_number)
{
   return {file_path, magic_number};
}

auto read_dir_entries(const fs::path& directory)
   -> tbb::concurrent_vector<std::pair<std::size_t, Ucfb_builder>>
{
   tbb::concurrent_vector<std::pair<std::size_t, Ucfb_builder>> entries;

   tbb::task_group tasks;

   for (const auto& entry : fs::directory_iterator{directory}) {
      const auto& path = entry.path();
      const auto entry_info = decompose_name(path.stem().string());

      if (fs::is_directory(path)) {
         tasks.run([&entries, entry_info, path] {
            entries.emplace_back(entry_info.index, assemble_directory(path));
         });
      }
      else if (fs::is_regular_file(path)) {
         tasks.run([&entries, entry_info, path] {
            entries.emplace_back(entry_info.index,
                                 read_saved_chunk(path, entry_info.magic_number));
         });
      }
      else {
         throw std::runtime_error{"Unexpected entry in directory: "s += path.string()};
      }
   }

   tasks.wait();

   return entries;
}

void sort_dir_entries(
   tbb::concurrent_vector<std::pair<std::size_t, Ucfb_builder>>& entries)
{
   std::sort(
      std::begin(entries), std::end(entries),
      [](const auto left, const auto right) { return (left.first < right.first); });
}

Ucfb_builder assemble_directory(const fs::path& directory)
{
   auto entries = read_dir_entries(directory);

   sort_dir_entries(entries);

   const auto info = decompose_name(directory.stem().string());
   Ucfb_builder builder{info.magic_number};

   for (const auto& entry : entries) {
      builder.add_child(std::move(entry.second));
   }

   return builder;
}
}

void assemble_chunks(fs::path directory, File_saver& file_saver)
{
   if (!fs::is_directory(directory)) {
      throw std::invalid_argument{"Directory does not exist."};
   }

   const auto entry = fs::directory_iterator{directory};

   const auto& path = entry->path();

   if (fs::is_directory(path)) {
      const auto root = assemble_directory(path);

      file_saver.save_file(root.create_buffer(), "", directory.stem().string(),
                           ".assembled"sv);
   }
   else {
      throw std::runtime_error{"Unexpected entry in directory: "s += path.string()};
   }
}
