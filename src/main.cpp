
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

namespace fs = std::filesystem;
using namespace std::literals;

const auto usage = R"(Usage: swbf-unmunge <options>

Options:)"s;

constexpr std::array common_layer_suffixes{"_1ctf"sv
                                           "_1flag"sv
                                           "_Buildings"sv
                                           "_Buildings01"sv
                                           "_Buildings02"sv
                                           "_CP-Assult"sv
                                           "_CP-Conquest"sv
                                           "_CP-VehicleSpawns"sv
                                           "_CP-VehicleSpawns"sv
                                           "_CPs"sv
                                           "_CommonDesign"sv
                                           "_CW-Ships"sv
                                           "_GCW-Ships"sv
                                           "_Damage"sv
                                           "_Damage01"sv
                                           "_Damage02"sv
                                           "_Death"sv
                                           "_DeathRegions"sv
                                           "_Design"sv
                                           "_Design001"sv
                                           "_Design002"sv
                                           "_Design01"sv
                                           "_Design02"sv
                                           "_Design1"sv
                                           "_Design2"sv
                                           "_Doors"sv
                                           "_Layer000"sv
                                           "_Layer001"sv
                                           "_Layer002"sv
                                           "_Layer003"sv
                                           "_Layer004"sv
                                           "_Light_RG"sv
                                           "_NewObjective"sv
                                           "_Objective"sv
                                           "_Platforms"sv
                                           "_Props"sv
                                           "_RainShadow"sv
                                           "_Roids"sv
                                           "_Roids01"sv
                                           "_Roids02"sv
                                           "_Shadow_RGN"sv
                                           "_Shadows"sv
                                           "_Shields"sv
                                           "_SoundEmmiters"sv
                                           "_SoundRegions"sv
                                           "_SoundSpaces"sv
                                           "_SoundTriggers"sv
                                           "_Temp"sv
                                           "_Tree"sv
                                           "_Trees"sv
                                           "_Vehicles"sv
                                           "_animations"sv
                                           "_campaign"sv
                                           "_collision"sv
                                           "_con"sv
                                           "_conquest"sv
                                           "_ctf"sv
                                           "_deathreagen"sv
                                           "_droids"sv
                                           "_eli"sv
                                           "_flags"sv
                                           "_gunship"sv
                                           "_hunt"sv
                                           "_invisocube"sv
                                           "_light_region"sv
                                           "_objects01"sv
                                           "_objects02"sv
                                           "_reflections"sv
                                           "_rumble"sv
                                           "_rumbles"sv
                                           "_sound"sv
                                           "_tdm"sv
                                           "_trees"sv
                                           "_turrets"sv
                                           "_xl"sv};

void extract_file(const App_options& options, fs::path path) noexcept
{
   try {

      Mapped_file file{path};
      File_saver file_saver{fs::path{path}.replace_extension("") += '/',
                            options.verbose()};
      Swbf_fnv_hashes swbf_hashes;

      if (!options.user_string_dict().empty()) {

         if (fs::exists(options.user_string_dict())) {
            try {
               read_swbf_fnv_hash_dictionary(swbf_hashes, options.user_string_dict());
            }
            catch (std::exception& e) {
               synced_cout::print(
                  "Error: Exception occured while reading string dictionary.\n   Path: "s,
                  options.user_string_dict(), '\n', "   Message: "s, e.what(), '\n');
            }
         }
         else {
            std::cout << "Error: file '"s << options.user_string_dict()
                      << "' does not exist\n";
         }
      }

      for (const auto& input_file : options.input_files()) {
         const auto name = fs::path{input_file}.stem().string();

         swbf_hashes.add(name);
         swbf_hashes.add("mapname.description."s += name);
         swbf_hashes.add("mapname.name."s += name);

         for (const auto& suffix : common_layer_suffixes) {
            swbf_hashes.add(std::string{name} += suffix);
         }
      }

      Ucfb_reader root_reader{file.bytes()};

      if (root_reader.magic_number() != "ucfb"_mn) {
         throw std::runtime_error{"Root chunk is not ucfb as expected."};
      }

      synced_cout::print("Processing File: "s, path.string(), '\n');

      handle_ucfb(static_cast<Ucfb_reader>(root_reader), options, file_saver,
                  swbf_hashes);
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

   CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   const auto processor = get_file_processor(app_options.tool_mode());

   tbb::parallel_for_each(input_files, [&app_options, &processor](const auto& file) {
      processor(app_options, file);
   });

   CoUninitialize();
}
