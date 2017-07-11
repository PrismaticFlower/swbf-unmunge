
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "swbf_fnv_hashes.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

using namespace std::literals;

#pragma warning(disable : 4200)

namespace {

struct Name_prop {
   Magic_number mn;
   std::uint32_t size;

   char value[];
};

static_assert(std::is_standard_layout_v<Name_prop>);
static_assert(sizeof(Name_prop) == 8);

struct Hash_prop {
   Magic_number type;
   std::uint32_t size;
   std::uint32_t hash;

   char value[];
};

static_assert(std::is_standard_layout_v<Hash_prop>);
static_assert(sizeof(Hash_prop) == 12);

void write_bracketed_str(std::string_view what, std::string& to)
{
   to += '[';
   to += what;
   to += "]\n\n"_sv;
}

void write_property(std::pair<std::string_view, std::string_view> prop_value,
                    std::string& to)
{
   to += prop_value.first;
   to += " = \""_sv;
   to += prop_value.second;
   to += "\"\n"_sv;
}

std::string_view get_named_value(const chunks::Object& object, std::uint32_t& head,
                                 Magic_number type)
{
   const auto& prop = view_type_as<Name_prop>(object.bytes[head]);

   if (prop.mn != type) {
      throw std::runtime_error{"Badly structured class"};
   }

   head += prop.size + 8;

   return {&prop.value[0], prop.size - 1};
}

auto read_property(const Hash_prop& property)
   -> std::pair<std::uint32_t, std::string_view>
{
   return {property.hash, {&property.value[0], property.size - 5}};
}

auto get_properties(const chunks::Object& object, const std::uint32_t start)
   -> std::vector<std::pair<std::uint32_t, std::string_view>>
{
   std::uint32_t head = start;
   const std::uint32_t end = object.size;

   std::vector<std::pair<std::uint32_t, std::string_view>> properties;
   properties.reserve(128);

   while (head < end) {
      const auto& property = view_type_as<Hash_prop>(object.bytes[head]);

      properties.emplace_back(read_property(property));

      head += property.size + 8;
      if (head % 4 != 0) head += (4 - (head % 4));
   }

   return properties;
}

auto find_geometry_name(
   const std::vector<std::pair<std::uint32_t, std::string_view>>& properties)
   -> std::optional<std::string>
{
   constexpr auto geometry_name_hash = 0x47c86b4aui32;

   const auto result =
      std::find_if(std::cbegin(properties), std::cend(properties),
                   [](const auto& prop) { return prop.first == 0x47c86b4a; });

   if (result != std::cend(properties)) return std::string{result->second} += ".msh"_sv;

   return std::nullopt;
}
}

void handle_object(Ucfb_reader object_reader, File_saver& file_saver,
                   std::string_view type)
{
   const auto& object = object_reader.view_as_chunk<chunks::Object>();

   std::string file_buffer;
   file_buffer.reserve(1024);

   write_bracketed_str(type, file_buffer);

   std::uint32_t head = 0;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   write_property({"ClassLabel"_sv, get_named_value(object, head, "BASE"_mn)},
                  file_buffer);

   align_head();

   const auto odf_name = get_named_value(object, head, "TYPE"_mn);

   align_head();

   const auto properties = get_properties(object, head);

   const auto geom_name = find_geometry_name(properties);

   if (geom_name) write_property({"GeometryName"_sv, *geom_name}, file_buffer);

   file_buffer += '\n';

   write_bracketed_str("Properties"s, file_buffer);

   for (const auto& property : properties) {
      write_property({lookup_fnv_hash(property.first), property.second}, file_buffer);
   }

   file_saver.save_file(std::move(file_buffer), std::string{odf_name} += ".odf"_sv,
                        "odf"s);
}