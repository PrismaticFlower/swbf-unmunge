
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "swbf_fnv_hashes.hpp"
#include "ucfb_reader.hpp"

#include <gsl/gsl>

#include <algorithm>
#include <cmath>
#include <string_view>

using namespace std::literals;

namespace {

constexpr auto precision_cutoff = 0.00001f;

inline std::string cast_number_value(const float number)
{
   const auto fraction = std::remainder(number, 1.0f);
   const auto absolute_fraction = std::abs(fraction);

   if (absolute_fraction < precision_cutoff)
      return std::to_string(static_cast<std::int64_t>(number));

   return std::to_string(number);
}

inline void remove_last_semicolen(std::string& buffer)
{
   if (buffer.size() >= 2) {
      if (*(buffer.cend() - 2) == ';') {
         buffer.pop_back();
         buffer.back() = '\n';
      }
   }
}

bool is_string_data(Ucfb_reader_strict<"DATA"_mn> data)
{
   data.consume(4);

   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   if (element_count == 0) return false;

   const auto str_sizes_size = data.read_trivial_unaligned<std::uint32_t>();

   if (str_sizes_size / 4 != element_count) return false;

   const auto str_sizes = data.read_array_unaligned<std::uint32_t>(element_count);

   const std::size_t str_array_size = str_sizes[element_count - 1];

   return (data.size() == 9 + str_sizes_size + str_array_size);
}

bool is_hash_data(Ucfb_reader_strict<"DATA"_mn> data)
{
   constexpr std::array hashes = {
      "GrassPatch"_fnv,
      "File"_fnv,
      "Sound"_fnv,
      "CollisionSound"_fnv,
      "Path"_fnv,
      "BorderOdf"_fnv,
      "LeafPatch"_fnv,
      "Name"_fnv,
      "Movie"_fnv,
      "Inherit"_fnv,
      "Segment"_fnv,
      "Font"_fnv,
      "Subtitle"_fnv,
      "BUS"_fnv,
      "Stream"_fnv,
      "SoundStream"_fnv,
      "Sample"_fnv,
      "Group"_fnv,
      "Class"_fnv,
      "FootstepLeftWalk"_fnv,
      "FootstepRightWalk"_fnv,
      "FootstepLeftRun"_fnv,
      "FootstepRightRun"_fnv,
      "FootstepLeftStop"_fnv,
      "FootstepRightStop"_fnv,
      "Jump"_fnv,
      "Land"_fnv,
      "Roll"_fnv,
      "Squat"_fnv,
      "BodyFall"_fnv,
      "I3DL2ReverbPreset"_fnv,
   };

   const auto data_hash = data.read_trivial<std::uint32_t>();
   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   if (element_count > 0) {
      return std::find(std::begin(hashes), std::end(hashes), data_hash) !=
             std::end(hashes);
   }

   return false;
}

bool is_hybrid_data(Ucfb_reader_strict<"DATA"_mn> data)
{
   data.consume(4);

   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   if (element_count != 2) return false;

   return (data.size() != (element_count * sizeof(float) + 9));
}

bool is_float_data(Ucfb_reader_strict<"DATA"_mn> data)
{
   data.consume(4);

   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   return ((element_count > 0) && (data.size() == (element_count * sizeof(float) + 9)));
}

std::string read_string_data(Ucfb_reader_strict<"DATA"_mn> data,
                             const Swbf_fnv_hashes& swbf_hashes,
                             const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += swbf_hashes.lookup(hash);
   line += '(';

   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();
   const auto str_sizes_size = data.read_trivial_unaligned<std::uint32_t>();
   const auto str_sizes = data.read_array_unaligned<std::uint32_t>(element_count);

   while (data) {
      line += '\"';
      line += data.read_string_unaligned();
      line += "\", "sv;
   }

   line.resize(line.size() - 2);
   line += ");\n"sv;

   return line;
}

std::string read_hash_data(Ucfb_reader_strict<"DATA"_mn> data,
                           const Swbf_fnv_hashes& swbf_hashes,
                           const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();
   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();
   const auto value_hash = data.read_trivial_unaligned<std::uint32_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += swbf_hashes.lookup(hash);
   line += "(\""sv;
   line += swbf_hashes.lookup(value_hash);
   line += "\", "sv;

   for (std::size_t i = 1; i < element_count; ++i) {
      line += cast_number_value(data.read_trivial_unaligned<float>());
      line += ", "sv;
   }

   line.resize(line.size() - 2);

   line += ");\n"sv;

   return line;
}

std::string read_hybrid_data(Ucfb_reader_strict<"DATA"_mn> data,
                             const Swbf_fnv_hashes& swbf_hashes,
                             const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();
   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   // data.consume_unaligned(4);
   const auto string_index = data.read_trivial_unaligned<std::uint32_t>();

   const auto value = data.read_trivial_unaligned<float>();

   const auto string_size = data.read_trivial_unaligned<std::uint32_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += swbf_hashes.lookup(hash);
   line += "(\""sv;
   line += data.read_string_unaligned();
   line += "\", "sv;
   line += cast_number_value(value);
   line += ");\n"sv;

   return line;
}

std::string read_float_data(Ucfb_reader_strict<"DATA"_mn> data,
                            const Swbf_fnv_hashes& swbf_hashes,
                            const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();
   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += swbf_hashes.lookup(hash);
   line += '(';

   for (std::size_t i = 0; i < element_count; ++i) {
      line += cast_number_value(data.read_trivial_unaligned<float>());
      line += ", "sv;
   }

   line.resize(line.size() - 2);

   line += ");\n"sv;

   return line;
}

std::string read_tag_data(Ucfb_reader_strict<"DATA"_mn> data,
                          const Swbf_fnv_hashes& swbf_hashes,
                          const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += swbf_hashes.lookup(hash);
   line += "();\n"sv;

   return line;
}

std::string read_data(Ucfb_reader_strict<"DATA"_mn> data,
                      const Swbf_fnv_hashes& swbf_hashes,
                      const std::size_t indention_level, bool strings_are_hashed)
{
   if (is_string_data(data)) {
      return read_string_data(data, swbf_hashes, indention_level);
   }
   else if (strings_are_hashed && is_hash_data(data)) {
      return read_hash_data(data, swbf_hashes, indention_level);
   }
   else if (is_hybrid_data(data)) {
      return read_hybrid_data(data, swbf_hashes, indention_level);
   }
   else if (is_float_data(data)) {
      return read_float_data(data, swbf_hashes, indention_level);
   }
   else {
      return read_tag_data(data, swbf_hashes, indention_level);
   }
}

std::string read_scope(Ucfb_reader_strict<"SCOP"_mn> scope,
                       const Swbf_fnv_hashes& swbf_hashes,
                       const std::size_t indention_level, bool strings_are_hashed)
{
   Expects(indention_level >= 1);

   std::string buffer;
   buffer.reserve(4096);

   buffer.append(indention_level - 1, '\t');
   buffer += "{\n"sv;

   while (scope) {
      const auto child = scope.read_child();

      if (child.magic_number() == "DATA"_mn) {
         buffer += read_data(Ucfb_reader_strict<"DATA"_mn>{child}, swbf_hashes,
                             indention_level, strings_are_hashed);
      }
      else if (child.magic_number() == "SCOP"_mn) {
         remove_last_semicolen(buffer);

         buffer += read_scope(Ucfb_reader_strict<"SCOP"_mn>{child}, swbf_hashes,
                              indention_level + 1, strings_are_hashed);
      }
   }

   buffer.append(indention_level - 1, '\t');
   buffer += "}\n\n"sv;

   return buffer;
}

std::string read_root_scope(Ucfb_reader config, const Swbf_fnv_hashes& swbf_hashes,
                            bool strings_are_hashed)
{
   std::string buffer;
   buffer.reserve(16384);

   while (config) {
      const auto child = config.read_child();

      if (child.magic_number() == "DATA"_mn) {
         buffer += read_data(Ucfb_reader_strict<"DATA"_mn>{child}, swbf_hashes, 0,
                             strings_are_hashed);
      }
      else if (child.magic_number() == "SCOP"_mn) {
         remove_last_semicolen(buffer);

         buffer += read_scope(Ucfb_reader_strict<"SCOP"_mn>{child}, swbf_hashes, 1,
                              strings_are_hashed);
      }
   }

   return buffer;
}
}

void handle_config(Ucfb_reader config, File_saver& file_saver,
                   const Swbf_fnv_hashes& swbf_hashes, std::string_view file_type,
                   std::string_view dir, bool strings_are_hashed)
{
   const auto name_hash =
      config.read_child_strict<"NAME"_mn>().read_trivial<std::uint32_t>();
   auto name = swbf_hashes.lookup(name_hash);

   auto buffer = read_root_scope(config, swbf_hashes, strings_are_hashed);

   if (!buffer.empty()) {
      file_saver.save_file(buffer, dir, name, file_type);
   }
}
