#pragma once

#include"tbb/concurrent_queue.h"

#include<condition_variable>
#include<filesystem>
#include<thread>
#include<vector>

class File_saver
{
public:
   File_saver(std::experimental::filesystem::path path) noexcept;

   ~File_saver();

   //Save data to a file asynchronously.
   void save_file(std::string contents, 
                  std::string name,
                  std::string directory) noexcept;

private:
   struct Path_info
   {
      std::string name;
      std::string directory;
   };

   using File_info = std::pair<std::string, Path_info>;

   void run() noexcept;

   void save(File_info info) noexcept;
   
   void create_dir(std::string_view directory) noexcept;

   std::mutex _running_mutex;
   bool _running = true;
   std::condition_variable _cond_var;
   std::thread _thread;

   std::experimental::filesystem::path _path;
   tbb::concurrent_queue<File_info> _file_queue;

   std::vector<std::string> _created_dirs;
};