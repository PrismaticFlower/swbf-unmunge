#pragma once

#include "tbb/spin_rw_mutex.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class File_saver {
public:
   File_saver(const std::experimental::filesystem::path& path) noexcept;

   void save_file(std::string_view contents, std::string_view directory,
                  std::string_view name, std::string_view extension);

private:
   void create_dir(std::string_view directory) noexcept;

   const std::string _path;

   tbb::spin_rw_mutex _dirs_mutex;
   std::vector<std::string> _created_dirs;
};