
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "swbf_fnv_hashes.hpp"
#include "ucfb_reader.hpp"

#include <gsl/gsl>

#include <algorithm>

namespace {

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
   const std::array<std::uint32_t, 7> hashes = {
      0x156b70a1, // GrassPatch
      0xaaea5743, // File
      0x0e0d9594, // Sound
      0xc28f0c96, // CollisionSound
      0x84874d36, // Path
      0x6850acc6, // BorderOdf
      0x6a6fb399  // LeafPatch
   };

   const auto data_hash = data.read_trivial<std::uint32_t>();
   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   return ((std::find(std::cbegin(hashes), std::cend(hashes), data_hash) !=
            std::cend(hashes)) &&
           element_count > 0);
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
                             const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += lookup_fnv_hash(hash);
   line += '(';

   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();
   const auto str_sizes_size = data.read_trivial_unaligned<std::uint32_t>();
   const auto str_sizes = data.read_array_unaligned<std::uint32_t>(element_count);

   while (data) {
      line += '\"';
      line += data.read_string_unaligned();
      line += "\", "_sv;
   }

   line.resize(line.size() - 2);
   line += ");\n"_sv;

   return line;
}

std::string read_hash_data(Ucfb_reader_strict<"DATA"_mn> data,
                           const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();
   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();
   const auto value_hash = data.read_trivial_unaligned<std::uint32_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += lookup_fnv_hash(hash);
   line += "(\""_sv;
   line += lookup_fnv_hash(value_hash);
   line += "\", "_sv;

   for (std::size_t i = 1; i < element_count; ++i) {
      line += std::to_string(data.read_trivial_unaligned<float>());
      line += ", "_sv;
   }

   line.resize(line.size() - 2);

   line += ");\n"_sv;

   return line;
}

std::string read_hybrid_data(Ucfb_reader_strict<"DATA"_mn> data,
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
   line += lookup_fnv_hash(hash);
   line += "(\""_sv;
   line += data.read_string_unaligned();
   line += "\", "_sv;
   line += std::to_string(value);
   line += ");\n"_sv;

   return line;
}

std::string read_float_data(Ucfb_reader_strict<"DATA"_mn> data,
                            const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();
   const auto element_count = data.read_trivial_unaligned<std::uint8_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += lookup_fnv_hash(hash);
   line += '(';

   for (std::size_t i = 0; i < element_count; ++i) {
      line += std::to_string(data.read_trivial_unaligned<float>());
      line += ", "_sv;
   }

   line.resize(line.size() - 2);

   line += ");\n"_sv;

   return line;
}

std::string read_tag_data(Ucfb_reader_strict<"DATA"_mn> data,
                          const std::size_t indention_level)
{
   const auto hash = data.read_trivial<std::uint32_t>();

   std::string line;
   line.append(indention_level, '\t');
   line += lookup_fnv_hash(hash);
   line += "();\n"_sv;

   return line;
}

std::string read_data(Ucfb_reader_strict<"DATA"_mn> data,
                      const std::size_t indention_level, bool strings_are_hashed)
{
   if (is_string_data(data)) {
      return read_string_data(data, indention_level);
   }
   else if (strings_are_hashed && is_hash_data(data)) {
      return read_hash_data(data, indention_level);
   }
   else if (is_hybrid_data(data)) {
      return read_hybrid_data(data, indention_level);
   }
   else if (is_float_data(data)) {
      return read_float_data(data, indention_level);
   }
   else {
      return read_tag_data(data, indention_level);
   }
}

std::string read_scope(Ucfb_reader_strict<"SCOP"_mn> scope,
                       const std::size_t indention_level, bool strings_are_hashed)
{
   Expects(indention_level >= 1);

   std::string buffer;
   buffer.reserve(4096);

   buffer.append(indention_level - 1, '\t');
   buffer += "{\n"_sv;

   while (scope) {
      const auto child = scope.read_child();

      if (child.magic_number() == "DATA"_mn) {
         buffer += read_data(Ucfb_reader_strict<"DATA"_mn>{child}, indention_level,
                             strings_are_hashed);
      }
      else if (child.magic_number() == "SCOP"_mn) {
         remove_last_semicolen(buffer);

         buffer += read_scope(Ucfb_reader_strict<"SCOP"_mn>{child}, indention_level + 1,
                              strings_are_hashed);
      }
   }

   buffer.append(indention_level - 1, '\t');
   buffer += "}\n"_sv;

   return buffer;
}

std::string read_root_scope(Ucfb_reader config, bool strings_are_hashed)
{
   std::string buffer;
   buffer.reserve(16384);

   while (config) {
      const auto child = config.read_child();

      if (child.magic_number() == "DATA"_mn) {
         buffer += read_data(Ucfb_reader_strict<"DATA"_mn>{child}, 0, strings_are_hashed);
      }
      else if (child.magic_number() == "SCOP"_mn) {
         remove_last_semicolen(buffer);

         buffer +=
            read_scope(Ucfb_reader_strict<"SCOP"_mn>{child}, 1, strings_are_hashed);
      }
   }

   return buffer;
}
}

void handle_config(Ucfb_reader config, File_saver& file_saver, std::string_view file_type,
                   std::string_view dir, bool strings_are_hashed)
{
   const auto name_hash =
      config.read_child_strict<"NAME"_mn>().read_trivial<std::uint32_t>();

   auto buffer = read_root_scope(config, strings_are_hashed);

   if (!buffer.empty()) {
      file_saver.save_file(std::move(buffer), std::to_string(name_hash) += file_type,
                           std::string{dir});
   }
}
