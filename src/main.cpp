
#include "app_options.hpp"
#include "chunk_processor.hpp"
#include "file_saver.hpp"
#include "mapped_file.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include "tbb/parallel_for_each.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <Windows.h>

namespace fs = std::experimental::filesystem;
using namespace std::literals;

const auto usage = R"(Usage: swbf-unmunge <options>

Options:)"s;

void process_file(const App_options& options, fs::path path) noexcept
{
   try {
      Mapped_file file{path};
      File_saver file_saver{fs::path{path}.replace_extension("") += '/'};
      msh::Builders_map msh_builders;

      Ucfb_reader root_reader{file.bytes()};

      process_chunk(root_reader, options, file_saver, msh_builders);

      msh::save_all(file_saver, msh_builders);
   }
   catch (std::exception& e) {
      std::cerr << "Error: " << e.what() << '\n';
   }
}

int main(int argc, char* argv[])
{
   std::ios_base::sync_with_stdio(false);

   if (argc == 1) {
      std::cout << usage;
      App_options{0, nullptr}.print_arguments(std::cout);
      std::cout << '\n';

      return EXIT_FAILURE;
   }

   const App_options app_options{argc, argv};

   const auto& input_files = app_options.input_files();

   if (input_files.empty()) {
      std::cout << "No input file specified."s;

      return 0;
   }

   CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   tbb::parallel_for_each(
      input_files, [&app_options](const auto& file) { process_file(app_options, file); });

   CoUninitialize();
}