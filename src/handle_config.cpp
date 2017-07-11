
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "swbf_fnv_hashes.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

#include <algorithm>
#include <array>
#include <stack>
#include <string>
#include <vector>

namespace {

#pragma pack(push, 1)
struct Data_chunk {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t hash;
};

static_assert(std::is_standard_layout_v<Data_chunk>);
static_assert(sizeof(Data_chunk) == 12);

struct Str_data_chunk {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t hash;
   std::uint8_t element_count;
   std::uint32_t str_sizes_size;
   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Str_data_chunk>);
static_assert(sizeof(Str_data_chunk) == 17);

struct Hybrid_data_chunk {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t hash;
   std::uint8_t element_count;
   std::uint32_t str_offset;
   float value;
   std::uint32_t str_length;
   char str[];
};

static_assert(std::is_standard_layout_v<Hybrid_data_chunk>);
static_assert(sizeof(Hybrid_data_chunk) == 25);

struct Hash_data_chunk {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t hash;
   std::uint8_t element_count;
   std::uint32_t value_hash;
   float floats[];
};

static_assert(std::is_standard_layout_v<Hash_data_chunk>);
static_assert(sizeof(Hash_data_chunk) == 17);

struct Float_data_chunk {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t hash;
   std::uint8_t element_count;
   float floats[];
};

static_assert(std::is_standard_layout_v<Float_data_chunk>);
static_assert(sizeof(Float_data_chunk) == 13);

#pragma pack(pop)

using Scope_stack = std::stack<std::uint32_t, std::vector<std::uint32_t>>;

void indent_line(std::size_t level, std::string& str)
{
   str.append(level, '\t');
}

bool is_str_data(const Data_chunk& data)
{
   const auto& str_data = view_type_as<Str_data_chunk>(data);

   if (str_data.element_count == 0) return false;
   if (str_data.str_sizes_size / 4 != str_data.element_count) return false;

   const auto* str_sizes = reinterpret_cast<const std::uint32_t*>(&str_data.bytes[0]);

   const std::size_t str_array_size = str_sizes[str_data.element_count - 1];

   return (str_data.size == 9 + str_data.str_sizes_size + str_array_size);
}

bool is_hybrid_data(const Data_chunk& data)
{
   const auto& hybrid_data = view_type_as<Hybrid_data_chunk>(data);

   if (hybrid_data.element_count != 2) return false;

   return (hybrid_data.size != (hybrid_data.element_count * sizeof(float) + 9));
}

bool is_hash_data(const Data_chunk& data)
{
   const auto& hash_data = view_type_as<Str_data_chunk>(data);

   const std::array<std::uint32_t, 7> hashes = {
      0x156b70a1, // GrassPatch
      0xaaea5743, // File
      0x0e0d9594, // Sound
      0xc28f0c96, // CollisionSound
      0x84874d36, // Path
      0x6850acc6, // BorderOdf
      0x6a6fb399  // LeafPatch
   };

   return ((std::find(std::cbegin(hashes), std::cend(hashes), hash_data.hash) !=
            std::cend(hashes)) &&
           hash_data.element_count > 0);
}

bool is_float_data(const Data_chunk& data)
{
   const auto& fl_data = view_type_as<Float_data_chunk>(data);

   return ((fl_data.element_count > 0) &&
           (fl_data.size == (fl_data.element_count * sizeof(float) + 9)));
}

std::string handle_str_data(const Str_data_chunk& data, std::uint32_t& parent_head)
{
   std::string line;
   line = lookup_fnv_hash(data.hash);
   line += '(';

   std::uint32_t head = data.str_sizes_size;
   std::uint32_t end = data.size - 9;

   while (head < end) {
      std::string_view str;
      str = reinterpret_cast<const char*>(&data.bytes[head]);
      head += static_cast<std::uint32_t>(str.size()) + 1;

      line += '\"';
      line += str;
      line += "\", "_sv;
   }

   line.resize(line.size() - 2);
   line += ");"_sv;

   parent_head += (data.size + 8);

   return line;
}

std::string handle_hybrid_data(const Hybrid_data_chunk& chunk, std::uint32_t& head)
{
   std::string line;
   line = lookup_fnv_hash(chunk.hash);
   line += "(\""_sv;
   line += std::string_view{chunk.str, chunk.str_length - 1};
   line += "\", "_sv;
   line += std::to_string(chunk.value);
   line += ");"_sv;

   head += (chunk.size + 8);

   return line;
}

std::string handle_hash_data(const Hash_data_chunk& chunk, std::uint32_t& head)
{
   std::string line;
   line = lookup_fnv_hash(chunk.hash);
   line += "(\""_sv;
   line += lookup_fnv_hash(chunk.value_hash);
   line += "\", "_sv;

   for (std::size_t i = 1; i < chunk.element_count; ++i) {
      line += std::to_string(chunk.floats[i]);
      line += ", "_sv;
   }

   line.resize(line.size() - 2);

   line += ");"_sv;

   head += (chunk.size + 8);

   return line;
}

std::string handle_float_data(const Float_data_chunk& chunk, std::uint32_t& head)
{
   std::string line;
   line = lookup_fnv_hash(chunk.hash);
   line += '(';

   for (std::size_t i = 0; i < chunk.element_count; ++i) {
      line += std::to_string(chunk.floats[i]);
      line += ", "_sv;
   }

   line.resize(line.size() - 2);
   line += ");"_sv;

   head += (chunk.size + 8);

   return line;
}

std::string handle_tag_data(const Float_data_chunk& chunk, std::uint32_t& head)
{
   std::string line;
   line = lookup_fnv_hash(chunk.hash);
   line += "();"_sv;
   head += (chunk.size + 8);

   return line;
}

std::string handle_data(const Data_chunk& chunk, std::uint32_t& head,
                        bool strings_are_hashed)
{
   if (is_str_data(chunk)) {
      return handle_str_data(view_type_as<Str_data_chunk>(chunk), head);
   }
   else if (strings_are_hashed && is_hash_data(chunk)) {
      return handle_hash_data(view_type_as<Hash_data_chunk>(chunk), head);
   }
   else if (is_hybrid_data(chunk)) {
      return handle_hybrid_data(view_type_as<Hybrid_data_chunk>(chunk), head);
   }
   else if (is_float_data(chunk)) {
      return handle_float_data(view_type_as<Float_data_chunk>(chunk), head);
   }
   else {
      return handle_tag_data(view_type_as<Float_data_chunk>(chunk), head);
   }
}
}

void handle_config(Ucfb_reader config, File_saver& file_saver, std::string_view file_type,
                   std::string_view dir, bool strings_are_hashed)
{
   const auto& chunk = config.view_as_chunk<chunks::Config>();

   Scope_stack scope_stack;
   std::string buffer;
   buffer.reserve(1024);

   std::uint32_t head = 0;
   const std::uint32_t end = chunk.size - 12;

   while (head < end) {
      const Data_chunk& child = view_type_as<Data_chunk>(chunk.bytes[head]);

      if (child.mn == "DATA"_mn) {
         const auto line = handle_data(child, head, strings_are_hashed);

         indent_line(scope_stack.size(), buffer);
         buffer.append(line);
         buffer += '\n';
      }
      else if (child.mn == "SCOP"_mn) {
         head += 8;

         if (!buffer.empty()) {
            buffer.resize(buffer.size() - 2);
            buffer += '\n';
         }

         indent_line(scope_stack.size(), buffer);
         buffer += "{\n"_sv;
         scope_stack.push(head + child.size);
      }

      if (head % 4 != 0) head += (4 - (head % 4));

      while (!scope_stack.empty() && head >= scope_stack.top()) {
         scope_stack.pop();
         indent_line(scope_stack.size(), buffer);
         buffer.append("}\n", 2);

         if (scope_stack.empty()) buffer += '\n';
      }
   }

   if (!buffer.empty()) {
      file_saver.save_file(std::move(buffer),
                           std::to_string(chunk.name_hash) += file_type,
                           std::string{dir});
   }
}
