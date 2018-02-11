#include "file_saver.hpp"
#include "synced_cout.hpp"

#include <gsl/gsl>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>

namespace fs = std::experimental::filesystem;
using namespace std::literals;

File_saver::File_saver(const fs::path& path, bool verbose) noexcept
   : _path{path.string()}, _verbose{verbose}
{
   fs::create_directory(_path);
}

File_saver::File_saver(File_saver&& other) noexcept
   : File_saver{other._path, other._verbose}
{
   std::lock_guard<tbb::spin_rw_mutex> lock{other._dirs_mutex};
   std::swap(_created_dirs, other._created_dirs);
}

void File_saver::save_file(std::string_view contents, std::string_view directory,
                           std::string_view name, std::string_view extension)
{
   const auto path = get_file_path(directory, name, extension);

   if (_verbose) {
      synced_cout::print("Info: Saving file \""s, path, '\"', '\n');
   }

   std::ofstream file{path, std::ios::binary};
   file.write(contents.data(), contents.size());
}

std::string File_saver::get_file_path(std::string_view directory, std::string_view name,
                                      std::string_view extension)
{
   create_dir(directory);

   std::string path;
   path.reserve(_path.length() + 1 + directory.length() + 1 + name.length() +
                extension.length());

   constexpr auto preferred_separator =
      static_cast<char>(std::experimental::filesystem::path::preferred_separator);

   path += _path;

   if (!directory.empty()) {
      path += directory;
      path += preferred_separator;
   }

   path += name;
   path += extension;

   return path;
}

void File_saver::create_dir(std::string_view directory) noexcept
{
   decltype(_created_dirs)::const_iterator dir_result;

   {
      const auto releaser = gsl::finally([this] { _dirs_mutex.unlock(); });
      _dirs_mutex.lock_read();

      dir_result =
         std::find(std::cbegin(_created_dirs), std::cend(_created_dirs), directory);
   }

   if (dir_result == std::cend(_created_dirs)) {
      std::string str_dir{directory};

      fs::path path{_path};
      path /= str_dir;

      std::lock_guard<tbb::spin_rw_mutex> writer_lock{_dirs_mutex};

      fs::create_directory(path);

      _created_dirs.emplace_back(std::move(str_dir));
   }
}

File_saver File_saver::create_nested(std::string_view directory) const
{
   fs::path new_path = _path;
   new_path.append(std::cbegin(directory), std::cend(directory));
   new_path += fs::path::preferred_separator;

   return {new_path, _verbose};
}
