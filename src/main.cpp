
#include "app_options.hpp"
#include "chunk_processor.hpp"
#include "file_saver.hpp"
#include "mapped_file.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include "tbb/task_group.h"

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <Windows.h>

namespace fs = std::experimental::filesystem;
using namespace std::literals;

const auto usage = R"(Usage: swbf-unmunge <options>

Options:)"s;

int main(int argc, char* argv[])
{
   std::ios_base::sync_with_stdio(false);

   if (argc == 1) {
      std::cout << usage;
      App_options{0, nullptr}.print_arguments(std::cout);
      std::cout << '\n';

      return EXIT_FAILURE;
   }

   try {
      const App_options app_options{argc, argv};
      const auto& file_path = app_options.input_file();

      if (file_path.empty()) throw std::runtime_error{"No input file specified."};

      CoInitializeEx(nullptr, COINIT_MULTITHREADED);

      Mapped_file file{file_path};
      File_saver file_saver{fs::path{file_path}.replace_extension("") += '/'};
      tbb::task_group tasks;
      msh::Builders_map msh_builders;

      Ucfb_reader root_reader{file.bytes()};

      tasks.run_and_wait([&, root_reader] {
         process_chunk(root_reader, app_options, file_saver, tasks, msh_builders);
      });

      msh::save_all(file_saver, msh_builders);

      CoUninitialize();

      return EXIT_SUCCESS;
   }
   catch (std::exception& e) {
      std::cerr << "Error: " << e.what() << '\n';

      CoUninitialize();

      return EXIT_FAILURE;
   }
}