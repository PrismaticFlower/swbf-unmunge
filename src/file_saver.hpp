#pragma once

#include "tbb/spin_rw_mutex.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class File_saver {
public:
   File_saver(const std::experimental::filesystem::path& path,
              bool verbose = false) noexcept;

   File_saver(File_saver&& other) noexcept;

   void save_file(std::string_view contents, std::string_view directory,
                  std::string_view name, std::string_view extension);

   File_saver create_nested(std::string_view directory) const;

private:
   void create_dir(std::string_view directory) noexcept;

   const std::string _path;
   const bool _verbose = false;

   tbb::spin_rw_mutex _dirs_mutex;
   std::vector<std::string> _created_dirs;
};