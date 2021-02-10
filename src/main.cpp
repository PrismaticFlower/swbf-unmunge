
#include "app_options.hpp"
#include "assemble_chunks.hpp"
#include "chunk_handlers.hpp"
#include "explode_chunk.hpp"
#include "file_saver.hpp"
#include "mapped_file.hpp"
#include "swbf_fnv_hashes.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include "tbb/parallel_for_each.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>

#include <Windows.h>
#include <regex>

namespace fs = std::filesystem;
using namespace std::literals;

const auto usage = R"(Usage: swbf-unmunge <options>

Options:)"s;

std::vector<std::string> popular_suffixes{"_1ctf",
                                         "_1flag",
                                         "_Buildings",
                                         "_Buildings",
                                         "_Buildings01",
                                         "_Buildings02",
                                         "_CP-Assult",
                                         "_CP-Conquest",
                                         "_CP-VehicleSpawns",
                                         "_CP-VehicleSpawns",
                                         "_CPs",
                                         "_CommonDesign",
                                         "_CW-Ships",
                                         "_GCW-Ships",
                                         "_Damage",
                                         "_Damage01",
                                         "_Damage02",
                                         "_Death",
                                         "_DeathRegions",
                                         "_Design",
                                         "_Design001",
                                         "_Design002",
                                         "_Design01",
                                         "_Design02",
                                         "_Design1",
                                         "_Design2",
                                         "_Doors",
                                         "_Layer000",
                                         "_Layer001",
                                         "_Layer002",
                                         "_Layer003",
                                         "_Layer004",
                                         "_Light_RG",
                                         "_NewObjective",
                                         "_Objective",
                                         "_Platforms",
                                         "_Props",
                                         "_RainShadow",
                                         "_Roids",
                                         "_Roids01",
                                         "_Roids02",
                                         "_Shadow_RGN",
                                         "_Shadows",
                                         "_Shields",
                                         "_SoundEmmiters",
                                         "_SoundRegions",
                                         "_SoundSpaces",
                                         "_SoundTriggers",
                                         "_Temp",
                                         "_Tree",
                                         "_Trees",
                                         "_Vehicles",
                                         "_animations",
                                         "_campaign",
                                         "_collision",
                                         "_con",
                                         "_conquest",
                                         "_ctf",
                                         "_deathreagen",
                                         "_droids",
                                         "_eli",
                                         "_flags",
                                         "_gunship",
                                         "_hunt",
                                         "_invisocube",
                                         "_light_region",
                                         "_objects01",
                                         "_objects02",
                                         "_reflections",
                                         "_rumble",
                                         "_rumbles",
                                         "_sound",
                                         "_tdm",
                                         "_trees",
                                         "_turrets",
                                         "_xl"};
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
      synced_cout::print("Processing File: "s, path.string(), '\n');

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
   if (app_options.user_string_dict().length() > 0) {
      std::string path =
         std::regex_replace(app_options.user_string_dict(), std::regex("\\\\"), "\\");
      path = std::regex_replace(path, std::regex("\""), "");
      if (fs::exists(path))
         read_dictionary(path.c_str());
      else {
         std::cout << "Error: file '"s << path << "' does not exist\n";

         return 0;
      }
   }
   else
      initialize_internal_dictionary();

   std::string tmp = "";
   int start_str = 0;
   int end_str = 0;
   
   // add the input file names and filename + 'popular suffixes' to the hashes
   for (int i = 0; i < input_files.size(); i++) {
      start_str = input_files[i].find_last_of('\\');
      if (start_str < 0) start_str = 0;
      end_str = input_files[i].find_last_of('.');
      tmp = input_files[i].substr(start_str, end_str - start_str);
      add_hash(tmp);
      add_hash("mapname.description." + tmp);
      add_hash("mapname.name." + tmp);
      for (int j = 0; j < popular_suffixes.size(); j++) {
         add_hash(tmp + popular_suffixes[j]);
      }
   }

   CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   const auto processor = get_file_processor(app_options.tool_mode());

   tbb::parallel_for_each(input_files, [&app_options, &processor](const auto& file) {
      processor(app_options, file);
   });

   CoUninitialize();
}
