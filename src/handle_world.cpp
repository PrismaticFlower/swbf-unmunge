
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "swbf_fnv_hashes.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_CXX98

#include "glm/gtc/quaternion.hpp"
#include "glm/mat3x3.hpp"
#include "glm/vec3.hpp"

#include "tbb/task_group.h"

#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std::literals;

namespace {

#pragma pack(push, 1)

struct Xframe {
   Magic_number mn;
   std::uint32_t size;

   glm::mat3 matrix;
   glm::vec3 position;
};

static_assert(std::is_standard_layout_v<Xframe>);
static_assert(sizeof(Xframe) == 56);

struct Size {
   Magic_number mn;
   std::uint32_t size;

   glm::vec3 vec;

   static_assert(std::is_standard_layout_v<glm::vec3>);
   static_assert(sizeof(glm::vec3) == 12);
};

static_assert(std::is_standard_layout_v<Size>);
static_assert(sizeof(Size) == 20);

struct Property {
   Magic_number mn;
   std::uint32_t size;

   std::uint32_t hash;

   char str[];
};

static_assert(std::is_standard_layout_v<Property>);
static_assert(sizeof(Property) == 12);

struct Entry {
   Magic_number mn;
   std::uint32_t size;

   std::uint32_t info_mn;
   std::uint32_t info_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Entry>);
static_assert(sizeof(Entry) == 16);

struct Name_value {
   Magic_number mn;
   std::uint32_t size;

   char str[];
};

static_assert(std::is_standard_layout_v<Name_value>);
static_assert(sizeof(Name_value) == 8);

struct Flag_value {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t flags;
};

static_assert(std::is_standard_layout_v<Flag_value>);
static_assert(sizeof(Flag_value) == 12);

struct Animation_key {
   static_assert(std::is_standard_layout_v<glm::vec3>);
   static_assert(sizeof(glm::vec3) == 12);

   Magic_number mn;
   std::uint32_t size;

   float time;
   glm::vec3 data;
   std::uint8_t type;
   float spline_data[6];
};

static_assert(std::is_standard_layout_v<Animation_key>);
static_assert(sizeof(Animation_key) == 49);

#pragma pack(pop)

const std::string_view world_header = {R"(Version(3);
SaveType(0);

Camera("camera")
{
	Rotation(1.000, 0.000, 0.000, 0.000);
	Position(0.000, 0.000, 0.000);
	FieldOfView(55.400);
	NearPlane(1.000);
	FarPlane(5000.000);
	ZoomFactor(1.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
	Bookmark(0.000, 0.000, 0.000,  1.000, 0.000, 0.000, 0.000);
}

ControllerManager("StandardCtrlMgr");

WorldExtents()
{
	Min(0.000000, 0.000000, 0.000000);
	Max(0.000000, 0.000000, 0.000000);
}
)"_sv};

std::string_view read_name_value(const Name_value& name_value)
{
   return {&name_value.str[0], name_value.size - 1};
}

void write_key_value(bool indent, std::string_view key, std::string_view value,
                     std::string& buffer)
{
   if (indent) buffer += '\t';

   if (string_is_number(value)) {
      buffer += key;
      buffer += "("_sv;
      buffer += value;
      buffer += ");\n"_sv;
   }
   else {
      buffer += key;
      buffer += "(\""_sv;
      buffer += value;
      buffer += "\");\n"_sv;
   }
}

void write_key_value(bool indent, std::string_view key, std::int64_t value,
                     std::string& buffer)
{
   if (indent) buffer += '\t';

   buffer += key;
   buffer += "("_sv;
   buffer += std::to_string(value);
   buffer += ");\n"_sv;
}

void write_key_value(bool indent, std::string_view key, glm::quat value,
                     std::string& buffer)
{
   if (indent) buffer += '\t';

   buffer += key;
   buffer += '(';
   buffer += std::to_string(value.w);
   buffer += ", "_sv;
   buffer += std::to_string(value.x);
   buffer += ", "_sv;
   buffer += std::to_string(value.y);
   buffer += ", "_sv;
   buffer += std::to_string(value.z);
   buffer += ");\n"_sv;
}

void write_key_value(bool indent, std::string_view key, glm::vec3 value,
                     std::string& buffer)
{
   if (indent) buffer += '\t';

   buffer += key;
   buffer += '(';
   buffer += std::to_string(value.x);
   buffer += ", "_sv;
   buffer += std::to_string(value.y);
   buffer += ", "_sv;
   buffer += std::to_string(value.z);
   buffer += ");\n"_sv;
}

char convert_region_type(std::string_view type)
{
   if (type == "box"_sv) return '0';
   if (type == "sphere"_sv) return '1';
   if (type == "cylinder"_sv) return '2';

   throw std::invalid_argument{"Invalid region type passed to function."};
}

std::pair<glm::quat, glm::vec3> convert_xframe(const Xframe& xframe)
{
   auto position = xframe.position;
   position.z *= -1.0f;

   auto quat = glm::quat_cast(xframe.matrix);
   quat *= glm::quat{0.0f, 0.0f, 1.0f, 0.0f};

   return {quat, position};
}

std::array<glm::vec3, 4> get_barrier_corners(const Xframe& xframe, const Size& size)
{
   std::array<glm::vec3, 4> corners = {{
      {size.vec.x, 0.0f, size.vec.z},
      {-size.vec.x, 0.0f, size.vec.z},
      {-size.vec.x, 0.0f, -size.vec.z},
      {size.vec.x, 0.0f, -size.vec.z},
   }};

   corners[0] = xframe.matrix * corners[0];
   corners[1] = xframe.matrix * corners[1];
   corners[2] = xframe.matrix * corners[2];
   corners[3] = xframe.matrix * corners[3];

   corners[0] += xframe.position;
   corners[1] += xframe.position;
   corners[2] += xframe.position;
   corners[3] += xframe.position;

   return corners;
}

void process_property(const Property& prop, std::uint32_t& head, std::string& buffer)
{
   const auto name = lookup_fnv_hash(prop.hash);

   write_key_value(true, name, {&prop.str[0], prop.size - 5}, buffer);

   head += prop.size + 8;
}

void process_region(const Entry& region, std::string& buffer)
{
   std::uint32_t head = 0;
   const std::uint32_t end = region.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   const auto type = read_name_value(view_type_as<Name_value>(region.bytes[head]));

   head += view_type_as<Name_value>(region.bytes[head]).size + 8;
   align_head();

   const auto name = read_name_value(view_type_as<Name_value>(region.bytes[head]));

   head += view_type_as<Name_value>(region.bytes[head]).size + 8;
   align_head();

   const auto& xframe = view_type_as<Xframe>(region.bytes[head]);
   head += sizeof(Xframe);

   const auto& size = view_type_as<Size>(region.bytes[head]);
   head += sizeof(Size);

   buffer += "Region(\""_sv;
   buffer += name;
   buffer += "\", "_sv;
   buffer += convert_region_type(type);
   buffer += ")\n{\n"_sv;

   const auto world_coords = convert_xframe(xframe);
   write_key_value(true, "Position"_sv, world_coords.second, buffer);
   write_key_value(true, "Rotation"_sv, world_coords.first, buffer);
   write_key_value(true, "Size"_sv, size.vec, buffer);

   while (head < end) {
      const auto& prop = view_type_as<Property>(region.bytes[head]);

      process_property(prop, head, buffer);

      align_head();
   }

   buffer += "}\n\n"_sv;
}

void process_barrier(const Entry& barrier, std::string& buffer)
{
   struct Barrier_info {
      Xframe xframe;
      Size size;
      Flag_value flag;
   };

   static_assert(std::is_standard_layout_v<Barrier_info>);
   static_assert(sizeof(Barrier_info) == 88);

   std::uint32_t head = 0;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   const auto name = read_name_value(view_type_as<Name_value>(barrier.bytes[head]));

   head += view_type_as<Name_value>(barrier.bytes[head]).size + 8;
   align_head();

   const auto& barrier_info = view_type_as<Barrier_info>(barrier.bytes[head]);
   head += sizeof(Barrier_info);

   const auto corners = get_barrier_corners(barrier_info.xframe, barrier_info.size);

   buffer += "Barrier(\""_sv;
   buffer += name;
   buffer += "\")\n{\n"_sv;

   for (const auto& corner : corners) {
      write_key_value(true, "Corner"_sv, corner, buffer);
   }

   write_key_value(true, "Flag"_sv, barrier_info.flag.flags, buffer);
   buffer += "}\n\n"_sv;
}

void process_hint(const Entry& hint, std::string& buffer)
{
   std::uint32_t head = 0;
   const std::uint32_t end = hint.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   const auto type = read_name_value(view_type_as<Name_value>(hint.bytes[head]));

   head += view_type_as<Name_value>(hint.bytes[head]).size + 8;
   align_head();

   const auto name = read_name_value(view_type_as<Name_value>(hint.bytes[head]));

   head += view_type_as<Name_value>(hint.bytes[head]).size + 8;
   align_head();

   const auto& xframe = view_type_as<Xframe>(hint.bytes[head]);
   head += sizeof(Xframe);

   buffer += "Hint(\""_sv;
   buffer += name;
   buffer += "\", \""_sv;
   buffer += type;
   buffer += "\")\n{\n"_sv;

   const auto world_coords = convert_xframe(xframe);
   write_key_value(true, "Position"_sv, world_coords.second, buffer);
   write_key_value(true, "Rotation"_sv, world_coords.first, buffer);

   while (head < end) {
      const auto& prop = view_type_as<Property>(hint.bytes[head]);

      process_property(prop, head, buffer);

      align_head();
   }

   buffer += "}\n\n"_sv;
}

void process_animation(const Entry& anim, std::string& buffer)
{
   std::uint32_t head = 0;
   const std::uint32_t end = anim.size - anim.info_size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   const std::uint32_t name_size = anim.info_size - 6;
   const std::string_view name{reinterpret_cast<const char*>(&anim.bytes[0]),
                               name_size - 1};

   head += name_size;

   const float length = view_type_as<float>(anim.bytes[head]);
   const std::int32_t unknown_flag_1 = view_type_as<std::uint8_t>(anim.bytes[head + 4]);
   const std::int32_t unknown_flag_2 = view_type_as<std::uint8_t>(anim.bytes[head + 5]);

   head += 6;
   align_head();

   buffer += "Animation(\""_sv;
   buffer += name;
   buffer += "\", "_sv;
   buffer += std::to_string(length);
   buffer += ", "_sv;
   buffer += std::to_string(unknown_flag_1);
   buffer += ", "_sv;
   buffer += std::to_string(unknown_flag_2);
   buffer += ")\n{\n"_sv;

   const auto write_anim_key = [&buffer, &head](std::string_view key_name,
                                                const Animation_key& key,
                                                auto&& mutator) {
      buffer += '\t';
      buffer += key_name;
      buffer += '(';
      buffer += std::to_string(key.time);
      buffer += ", "_sv;
      buffer += std::to_string(mutator(key.data.x));
      buffer += ", "_sv;
      buffer += std::to_string(mutator(key.data.y));
      buffer += ", "_sv;
      buffer += std::to_string(mutator(key.data.z));
      buffer += ", "_sv;
      buffer += std::to_string(static_cast<std::int16_t>(key.type));

      for (const auto& fl : key.spline_data) {
         buffer += ", "_sv;
         buffer += std::to_string(mutator(fl));
      }

      buffer.resize(buffer.size() - 2);

      buffer += ");\n"_sv;

      head += sizeof(Animation_key);
   };

   while (head < end) {
      const auto& key = view_type_as<Animation_key>(anim.bytes[head]);

      if (key.mn == "ROTK"_mn) {
         write_anim_key("AddRotationKey"_sv, key, glm::degrees<float>);
      }
      else if (key.mn == "POSK"_mn) {
         write_anim_key("AddPositionKey"_sv, key, [](auto&& fl) { return fl; });
      }

      align_head();
   }

   buffer += "}\n\n"_sv;
}

void process_animation_group(const Entry& anmg, std::string& buffer)
{
   std::uint32_t head = 0;
   const std::uint32_t end = anmg.size - anmg.info_size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   const std::uint32_t name_size = anmg.info_size - 2;
   const std::string_view name{reinterpret_cast<const char*>(&anmg.bytes[0]),
                               name_size - 1};

   head += name_size;

   const std::int32_t unknown_flag_1 = view_type_as<std::uint8_t>(anmg.bytes[head]);
   const std::int32_t unknown_flag_2 = view_type_as<std::uint8_t>(anmg.bytes[head + 1]);

   head += 2;
   align_head();

   buffer += "AnimationGroup(\""_sv;
   buffer += name;
   buffer += "\", "_sv;
   buffer += std::to_string(unknown_flag_1);
   buffer += ", "_sv;
   buffer += std::to_string(unknown_flag_2);
   buffer += ")\n{\n"_sv;

   while (head < end) {
      const auto& name_pair = view_type_as<Name_value>(anmg.bytes[head]);

      const std::size_t first_len = std::strlen(&name_pair.str[0]) + 1;
      const std::size_t second_len = name_pair.size - first_len;

      std::string_view first{&name_pair.str[0], first_len - 1};
      std::string_view second{&name_pair.str[first_len], second_len - 1};

      buffer += "\tAnimation(\""_sv;
      buffer += first;
      buffer += "\", \""_sv;
      buffer += second;
      buffer += "\");\n"_sv;

      head += name_pair.size + 8;
      align_head();
   }

   buffer += "}\n\n"_sv;
}

void process_animation_hierarchy(const Entry& anmh, std::string& buffer)
{
   const std::size_t string_count = view_type_as<std::uint8_t>(anmh.bytes[0]);
   const char* str_array = reinterpret_cast<const char*>(&anmh.bytes[1]);

   std::vector<std::string_view> strings;

   for (std::size_t i = 0; i < string_count; ++i) {
      const std::size_t str_len = std::strlen(str_array);

      strings.emplace_back(str_array, str_len);

      str_array += (str_len + 1);
   }

   buffer += "Hierarchy(\""_sv;
   buffer += strings[0];
   buffer += "\")\n{\n"_sv;

   for (std::size_t i = 1; i < strings.size(); ++i) {
      buffer += "\tObj(\""_sv;
      buffer += strings[i];
      buffer += "\");\n"_sv;
   }

   buffer += "}\n\n"_sv;
}

void process_instance(const Entry& instance, std::string& buffer)
{
   std::uint32_t head = 0;
   const std::uint32_t end = instance.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   const auto type = read_name_value(view_type_as<Name_value>(instance.bytes[head]));

   head += view_type_as<Name_value>(instance.bytes[head]).size + 8;
   align_head();

   const auto name = read_name_value(view_type_as<Name_value>(instance.bytes[head]));

   head += view_type_as<Name_value>(instance.bytes[head]).size + 8;
   align_head();

   buffer += "Object(\""_sv;
   buffer += name;
   buffer += "\", \""_sv;
   buffer += type;
   buffer += "\", 1)\n{\n"_sv;

   const auto& xframe = view_type_as<Xframe>(instance.bytes[head]);
   head += sizeof(Xframe);

   const auto world_coords = convert_xframe(xframe);
   write_key_value(true, "ChildRotation"_sv, world_coords.first, buffer);
   write_key_value(true, "ChildPosition"_sv, world_coords.second, buffer);

   while (head < end) {
      const auto& prop = view_type_as<Property>(instance.bytes[head]);

      process_property(prop, head, buffer);

      align_head();
   }

   buffer += "}\n\n"_sv;
}

void process_region_entries(std::vector<const Entry*> entries, std::string_view name,
                            File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(256 * entries.size());
   buffer += "Version(1);\n"_sv;

   write_key_value(false, "RegionCount"_sv, entries.size(), buffer);
   buffer += '\n';

   for (const auto* entry : entries) {
      process_region(*entry, buffer);
   }

   file_saver.save_file(std::move(buffer), std::string{name} += ".rgn"_sv, "world"s);
}

void process_instance_entries(std::vector<const Entry*> entries, const std::string name,
                              const std::string terrain_name, const std::string sky_name,
                              File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve((world_header.length() + 256) + 256 * entries.size());

   buffer += world_header;
   buffer += '\n';

   if (!terrain_name.empty())
      write_key_value(false, "TerrainName"_sv, terrain_name + ".ter"s, buffer);
   if (!sky_name.empty())
      write_key_value(false, "SkyName"_sv, sky_name + ".sky"s, buffer);

   write_key_value(false, "LightName"_sv, name + ".lgt"s, buffer);
   buffer += '\n';

   for (const auto* entry : entries) {
      process_instance(*entry, buffer);
   }

   std::string extension = ".wld"s;

   if (terrain_name.empty() || sky_name.empty()) extension = ".lyr"s;

   file_saver.save_file(std::move(buffer), name + extension, "world"s);
}

void process_barrier_entries(std::vector<const Entry*> entries, std::string_view name,
                             File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(256 * entries.size());

   write_key_value(false, "BarrierCount"_sv, entries.size(), buffer);
   buffer += '\n';

   for (const auto* entry : entries) {
      process_barrier(*entry, buffer);
   }

   file_saver.save_file(std::move(buffer), std::string{name} += ".bar"_sv, "world"s);
}

void process_hint_entries(std::vector<const Entry*> entries, std::string_view name,
                          File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(256 * entries.size());

   for (const auto* entry : entries) {
      process_hint(*entry, buffer);
   }

   file_saver.save_file(std::move(buffer), std::string{name} += ".hnt"_sv, "world"s);
}

void process_animation_entries(std::vector<const Entry*> entries, std::string_view name,
                               File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(512 * entries.size());

   for (const auto* entry : entries) {
      if (entry->mn == "anim"_mn) {
         process_animation(*entry, buffer);
      }
      else if (entry->mn == "anmg"_mn) {
         process_animation_group(*entry, buffer);
      }
      else if (entry->mn == "anmh"_mn) {
         process_animation_hierarchy(*entry, buffer);
      }
   }

   file_saver.save_file(std::move(buffer), std::string{name} += ".anm"_sv, "world"s);
}
}

void handle_world(Ucfb_reader world_reader, tbb::task_group& tasks,
                  File_saver& file_saver)
{
   const auto& world = world_reader.view_as_chunk<chunks::World>();

   std::string_view name;
   std::string_view terrain_name;
   std::string_view sky_name;

   std::uint32_t head = 0;
   const std::uint32_t end = world.size;

   std::vector<const Entry*> region_entries;
   std::vector<const Entry*> instance_entries;
   std::vector<const Entry*> barrier_entries;
   std::vector<const Entry*> hint_entries;
   std::vector<const Entry*> animation_entries;

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(world.bytes[head]);

      if (chunk.mn == "regn"_mn) {
         region_entries.emplace_back(&view_type_as<Entry>(chunk));
      }
      else if (chunk.mn == "inst"_mn) {
         instance_entries.emplace_back(&view_type_as<Entry>(chunk));
      }
      else if (chunk.mn == "BARR"_mn) {
         barrier_entries.emplace_back(&view_type_as<Entry>(chunk));
      }
      else if (chunk.mn == "Hint"_mn) {
         hint_entries.emplace_back(&view_type_as<Entry>(chunk));
      }
      else if (chunk.mn == "anim"_mn) {
         animation_entries.emplace_back(&view_type_as<Entry>(chunk));
      }
      else if (chunk.mn == "anmg"_mn) {
         animation_entries.emplace_back(&view_type_as<Entry>(chunk));
      }
      else if (chunk.mn == "anmh"_mn) {
         animation_entries.emplace_back(&view_type_as<Entry>(chunk));
      }
      else if (chunk.mn == "NAME"_mn) {
         name = read_name_value(view_type_as<Name_value>(chunk));
      }
      else if (chunk.mn == "TNAM"_mn) {
         terrain_name = read_name_value(view_type_as<Name_value>(chunk));
      }
      else if (chunk.mn == "SNAM"_mn) {
         sky_name = read_name_value(view_type_as<Name_value>(chunk));
      }

      head += chunk.size + 8;

      if (head % 4 != 0) head += (4 - (head % 4));
   }

   tasks.run([ region_entries{std::move(region_entries)}, name, &file_saver ] {
      process_region_entries(region_entries, name, file_saver);
   });

   tasks.run([
      instance_entries{std::move(instance_entries)}, name, terrain_name, sky_name,
      &file_saver
   ] {
      process_instance_entries(instance_entries, std::string{name},
                               std::string{terrain_name}, std::string{sky_name},
                               file_saver);
   });

   tasks.run([ barrier_entries{std::move(barrier_entries)}, name, &file_saver ] {
      process_barrier_entries(barrier_entries, name, file_saver);
   });

   tasks.run([ hint_entries{std::move(hint_entries)}, name, &file_saver ] {
      process_hint_entries(hint_entries, name, file_saver);
   });

   if (!animation_entries.empty()) {
      tasks.run([ animation_entries{std::move(animation_entries)}, name, &file_saver ] {
         process_animation_entries(animation_entries, name, file_saver);
      });
   }
}