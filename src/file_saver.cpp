#include "file_saver.hpp"

#include <algorithm>
#include <fstream>

namespace fs = std::experimental::filesystem;

File_saver::File_saver(std::experimental::filesystem::path path) noexcept
{
   _path = std::move(path);
   _thread = std::thread{[this] { run(); }};

   fs::create_directory(_path);
}

File_saver::~File_saver()
{
   {
      std::lock_guard<std::mutex> lock{_running_mutex};

      _running = false;
   }

   _cond_var.notify_one();

   _thread.join();
}

void File_saver::save_file(std::string contents, std::string name,
                           std::string directory) noexcept
{
   Path_info path_info;
   path_info.name = std::move(name);
   path_info.directory = std::move(directory);

   _file_queue.emplace(std::make_pair(std::move(contents), std::move(path_info)));

   _cond_var.notify_one();
}

void File_saver::run() noexcept
{
   while (true) {
      std::unique_lock<std::mutex> lock{_running_mutex};

      _cond_var.wait(lock, [this] { return (!_running || !_file_queue.empty()); });

      File_info info;

      while (_file_queue.try_pop(info)) save(std::move(info));

      if (!_running) break;
   }
}

void File_saver::save(File_info info) noexcept
{
   create_dir(info.second.directory);

   fs::path path = _path;

   path /= info.second.directory;
   path /= info.second.name;

   std::ofstream file{path, std::ios::binary | std::ios::out};

   file.write(info.first.data(), info.first.size());
}

void File_saver::create_dir(std::string_view directory) noexcept
{
   const auto dir_result =
      std::find(std::cbegin(_created_dirs), std::cend(_created_dirs), directory);

   if (dir_result == std::cend(_created_dirs)) {
      std::string str_dir{directory};

      fs::path path{_path};
      path /= str_dir;

      fs::create_directory(path);

      _created_dirs.emplace_back(std::move(str_dir));
   }
}
