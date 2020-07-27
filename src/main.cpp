
#include "app_options.hpp"
#include "assemble_chunks.hpp"
#include "chunk_handlers.hpp"
#include "explode_chunk.hpp"
#include "file_saver.hpp"
#include "mapped_file.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include "tbb/parallel_for_each.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>

#if defined(__linux__) || defined(__APPLE__)
//#include <Windows.h>
#endif

#define COUT(x) std::cout << x << std::endl;

namespace fs = std::filesystem;
using namespace std::literals;

const auto usage = R"(Usage: swbf-unmunge <options>

Options:)"s;

void extract_file(const App_options& options, fs::path path) noexcept
{
   try {
      Mapped_file file{path};
      File_saver file_saver{fs::path{path}.replace_extension("") += '/',
                            options.verbose()};

      Ucfb_reader root_reader{file.bytes()};

      if (root_reader.magic_number() != "ucfb"_mn) {
         throw std::runtime_error{"Root chunk is now ucfb as expected."};
      }

      handle_ucfb(static_cast<Ucfb_reader>(root_reader), options, file_saver);
   }
   catch (std::exception& e) {
      synced_cout::print("Error: Exception occured while processing file.\n   File: "s,
                         path.string(), '\n', "   Message: "s, e.what(), '\n');
   }
}

void explode_file(const App_options& options, fs::path path) noexcept
{
   try {
      Mapped_file file{path};
      File_saver file_saver{fs::path{path}.replace_extension("") += '/',
                            options.verbose()};

      Ucfb_reader root_reader{file.bytes()};

      explode_chunk(root_reader, file_saver);
   }
   catch (std::exception& e) {
      synced_cout::print("Error: Exception occured while processing file.\n   File: "s,
                         path.string(), '\n', "   Message: "s, e.what(), '\n');
   }
}

void assemble_directory(const App_options& options, fs::path path) noexcept
{
   try {
      File_saver file_saver{fs::path{path}.replace_extension("") /= "../",
                            options.verbose()};

      assemble_chunks(path, file_saver);
   }
   catch (std::exception& e) {
      synced_cout::print(
         "Error: Exception occured while assembling directory.\n   Directory: "s,
         path.string(), '\n', "   Message: "s, e.what(), '\n');
   }
}

auto get_file_processor(const Tool_mode mode)
   -> std::function<void(const App_options&, fs::path)>
{
   if (mode == Tool_mode::extract) return extract_file;
   if (mode == Tool_mode::explode) return explode_file;
   if (mode == Tool_mode::assemble) return assemble_directory;

   throw std::invalid_argument{""};
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
      std::cout << "Error: No input file specified.\n"s;

      return 0;
   }

   //CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   const auto processor = get_file_processor(app_options.tool_mode());

   COUT("Starting parse")

   tbb::parallel_for_each(input_files, [&app_options, &processor](const auto& file) {
      processor(app_options, file);
   });

   COUT("End parse")

   //CoUninitialize();
}
