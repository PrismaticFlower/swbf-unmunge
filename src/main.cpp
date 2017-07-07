
#include"chunk_headers.hpp"
#include"chunk_processor.hpp"
#include"file_saver.hpp"
#include"mapped_file.hpp"
#include"msh_builder.hpp"
#include"type_pun.hpp"

#include"tbb/task_group.h"

#include<cstddef>
#include<cstdlib>
#include<exception>
#include<filesystem>
#include<iostream>
#include<stdexcept>

namespace fs = std::experimental::filesystem;

#include<Windows.h>

int main(int argc, char* argv[])
{
   fs::path file_path;

   if (argc == 1) {
      std::cout << "Enter the name of a file to unmunge: \n";
      std::cin >> file_path;
   }
   else {
      file_path = argv[1];
   }
   
   CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   try {
      Mapped_file file{file_path};
      File_saver file_saver{file_path.replace_extension("") += '/'};
      tbb::task_group tasks; 
      msh::Builders_map msh_builders;

      const auto& root_chunk = view_type_as<chunks::Unknown>(*file.get_bytes());

      tasks.run_and_wait([&] {process_chunk(root_chunk, file_saver, tasks, msh_builders);});

      msh::save_all(file_saver, msh_builders);
   }
   catch (std::exception& e) {
      std::cerr << "Exception Occured: " << e.what() << '\n';

      CoUninitialize();

      return EXIT_FAILURE;
   }

   CoUninitialize();

   return EXIT_SUCCESS;
}