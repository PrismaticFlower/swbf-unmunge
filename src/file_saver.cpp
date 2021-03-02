#include "file_saver.hpp"
#include "app_options.hpp"
#include "synced_cout.hpp"

#include <gsl/gsl>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>

namespace fs = std::filesystem;
using namespace std::literals;

File_saver::File_saver(const fs::path& path, bool verbose) noexcept
   : _path{path.lexically_normal()}, _verbose{verbose}
{
   if (get_pre_processing_global()) return; // ---------early return----------------
   fs::create_directory(_path);
}

void File_saver::save_file(std::string_view contents, std::string_view directory,
                           std::string_view name, std::string_view extension)
{
   if (get_pre_processing_global()) return; // ---------early return----------------

   const auto path = directory.empty() ? build_file_path(name, extension)
                                       : build_file_path(directory, name, extension);

   create_dir(directory);

   if (_verbose) {
      synced_cout::print("Info: Saving file "s, path, '\n');
   }

   std::ofstream file{path, std::ios::binary};
   file.write(contents.data(), contents.size());
}

auto File_saver::open_save_file(std::string_view directory, std::string_view name,
                                std::string_view extension,
                                std::ios_base::openmode openmode) -> std::ofstream
{

   const auto path = directory.empty() ? build_file_path(name, extension)
                                       : build_file_path(directory, name, extension);

   create_dir(directory);

   if (_verbose) {
      synced_cout::print("Info: Saving file "s, path, '\n');
   }

   return std::ofstream{path, openmode};
}

auto File_saver::build_file_path(std::string_view directory, std::string_view name,
                                 std::string_view extension) -> fs::path
{
   return (_path / directory /= name) += extension;
}

auto File_saver::build_file_path(std::string_view name, std::string_view extension)
   -> fs::path
{
   return (_path / name) += extension;
}

void File_saver::create_dir(std::string_view directory) noexcept
{
   if (get_pre_processing_global()) return; // ---------early return----------------
   const auto dir_result = [&] {
      std::shared_lock lock{_dirs_mutex};

      return std::find(std::cbegin(_created_dirs), std::cend(_created_dirs), directory);
   }();

   if (dir_result == std::cend(_created_dirs)) {
      std::lock_guard lock{_dirs_mutex};

      fs::path path{_path};
      path /= directory;

      fs::create_directories(path);

      _created_dirs.emplace_back(directory);
   }
}

auto File_saver::create_nested(std::string_view directory) const -> File_saver
{
   fs::path new_path = _path;
   new_path.append(std::cbegin(directory), std::cend(directory));
   new_path += fs::path::preferred_separator;

   return {new_path, _verbose};
}
