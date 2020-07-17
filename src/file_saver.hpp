#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <shared_mutex>
#include <string>
#include <vector>

class File_saver {
public:
   File_saver(const std::filesystem::path& path, bool verbose = false) noexcept;

   void save_file(std::string_view contents, std::string_view directory,
                  std::string_view name, std::string_view extension);

   auto open_save_file(std::string_view directory, std::string_view name,
                       std::string_view extension,
                       std::ios_base::openmode openmode = std::ios::binary)
      -> std::ofstream;

   auto build_file_path(std::string_view directory, std::string_view name,
                        std::string_view extension) -> std::filesystem::path;

   auto build_file_path(std::string_view name, std::string_view extension)
      -> std::filesystem::path;

   void create_dir(std::string_view directory) noexcept;

   auto create_nested(std::string_view directory) const -> File_saver;

private:
   const std::filesystem::path _path;
   const bool _verbose = false;

   std::shared_mutex _dirs_mutex;
   std::vector<std::string> _created_dirs;
};
