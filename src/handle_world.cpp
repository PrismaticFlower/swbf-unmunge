
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "swbf_fnv_hashes.hpp"
#include "ucfb_reader.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <tbb/task_group.h>

#include <fmt/format.h>

using namespace std::literals;

namespace {

struct Transform {
   glm::vec3 rotation_x;
   glm::vec3 rotation_y;
   glm::vec3 rotation_z;
   glm::vec3 position;
};

static_assert(std::is_trivially_copyable_v<Transform>);
static_assert(sizeof(Transform) == 48);

struct Animation_key {
   float time;
   std::array<float, 3> data;
   std::uint8_t type;
   std::array<float, 6> spline_data;
};

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
)"sv};

void write_key_value(bool indent, bool quoted, std::string_view key,
                     std::string_view value, std::string& buffer)
{
   if (indent) buffer += '\t';

   if (quoted) {
      buffer += key;
      buffer += "(\""sv;
      buffer += value;
      buffer += "\");\n"sv;
   }
   else {
      buffer += key;
      buffer += "("sv;
      buffer += value;
      buffer += ");\n"sv;
   }
}

void write_key_value(bool indent, std::string_view key, std::int64_t value,
                     std::string& buffer)
{
   if (indent) buffer += '\t';

   buffer += key;
   buffer += "("sv;
   buffer += std::to_string(value);
   buffer += ");\n"sv;
}

void write_key_value(bool indent, std::string_view key, glm::quat value,
                     std::string& buffer)
{
   if (indent) buffer += '\t';

   buffer += key;
   buffer += '(';
   buffer += std::to_string(value.w);
   buffer += ", "sv;
   buffer += std::to_string(value.x);
   buffer += ", "sv;
   buffer += std::to_string(value.y);
   buffer += ", "sv;
   buffer += std::to_string(value.z);
   buffer += ");\n"sv;
}

void write_key_value(bool indent, std::string_view key, glm::vec3 value,
                     std::string& buffer)
{
   if (indent) buffer += '\t';

   buffer += key;
   buffer += '(';
   buffer += std::to_string(value.x);
   buffer += ", "sv;
   buffer += std::to_string(value.y);
   buffer += ", "sv;
   buffer += std::to_string(value.z);
   buffer += ");\n"sv;
}

void write_animation_key(std::string_view key, Animation_key value, std::string& buffer)
{
   buffer += '\t';
   buffer += key;
   buffer += '(';
   buffer += std::to_string(value.time);
   buffer += ", "sv;
   buffer += std::to_string(value.data[0]);
   buffer += ", "sv;
   buffer += std::to_string(value.data[1]);
   buffer += ", "sv;
   buffer += std::to_string(value.data[2]);
   buffer += ", "sv;
   buffer += std::to_string(static_cast<std::int16_t>(value.type));

   for (const auto& fl : value.spline_data) {
      buffer += ", "sv;
      buffer += std::to_string(fl);
   }

   buffer.resize(buffer.size() - 2);

   buffer += ");\n"sv;
}

char convert_region_type(std::string_view type)
{
   if (type == "box"sv) return '0';
   if (type == "sphere"sv) return '1';
   if (type == "cylinder"sv) return '2';

   throw std::invalid_argument{"Invalid region type passed to function."};
}

std::pair<glm::quat, glm::vec3> convert_transform(const Transform& transform)
{
   auto position = transform.position;
   position.z *= -1.0f;

   auto rotation = glm::quat{
      glm::mat3{transform.rotation_x, transform.rotation_x, transform.rotation_z}};

   rotation.x = -rotation.x;
   rotation.z = -rotation.z;

   std::swap(rotation.x, rotation.z);
   std::swap(rotation.y, rotation.w);

   return {rotation, position};
}

std::array<glm::vec3, 4> get_barrier_corners(const Transform& transform,
                                             const glm::vec3 size)
{
   std::array<glm::vec3, 4> corners = {{
      {size.x, 0.0f, size.z},
      {-size.x, 0.0f, size.z},
      {-size.x, 0.0f, -size.z},
      {size.x, 0.0f, -size.z},
   }};

   const auto rotation =
      glm::mat3{transform.rotation_x, transform.rotation_x, transform.rotation_z};

   corners[0] = corners[0] * rotation * glm::vec3{1.0f, 1.0f, -1.0f};
   corners[1] = corners[1] * rotation * glm::vec3{1.0f, 1.0f, -1.0f};
   corners[2] = corners[2] * rotation * glm::vec3{1.0f, 1.0f, -1.0f};
   corners[3] = corners[3] * rotation * glm::vec3{1.0f, 1.0f, -1.0f};

   const glm::vec3 position = transform.position;

   corners[0] += position;
   corners[1] += position;
   corners[2] += position;
   corners[3] += position;

   return corners;
}

void read_property(Ucfb_reader_strict<"PROP"_mn> property,
                   const Swbf_fnv_hashes& swbf_hashes, std::string& buffer)
{
   const auto hash = property.read_trivial<std::uint32_t>();
   const auto value = property.read_string();

   write_key_value(true, !string_is_number(value), swbf_hashes.lookup(hash), value,
                   buffer);
}

template<typename Quoted_filter>
void read_property(Ucfb_reader_strict<"PROP"_mn> property,
                   const Swbf_fnv_hashes& swbf_hashes, std::string& buffer,
                   const Quoted_filter& filter)
{
   const auto hash = property.read_trivial<std::uint32_t>();
   const auto value = property.read_string();

   const bool quoted = filter(hash);

   write_key_value(true, quoted, swbf_hashes.lookup(hash), value, buffer);
}

void read_region(Ucfb_reader_strict<"regn"_mn> region, const Swbf_fnv_hashes& swbf_hashes,
                 std::string& buffer)
{
   auto info = region.read_child_strict<"INFO"_mn>();

   const auto type = info.read_child_strict<"TYPE"_mn>().read_string();
   const auto name = info.read_child_strict<"NAME"_mn>().read_string();

   const auto transform = info.read_child_strict<"XFRM"_mn>().read_trivial<Transform>();

   const glm::vec3 size = info.read_child_strict<"SIZE"_mn>().read_trivial<glm::vec3>();

   buffer += "Region(\""sv;
   buffer += name;
   buffer += "\", "sv;
   buffer += convert_region_type(type);
   buffer += ")\n{\n"sv;

   const auto world_coords = convert_transform(transform);
   write_key_value(true, "Position"sv, world_coords.second, buffer);
   write_key_value(true, "Rotation"sv, world_coords.first, buffer);
   write_key_value(true, "Size"sv, size, buffer);

   while (region) {
      auto property = region.read_child_strict<"PROP"_mn>();

      read_property(property, swbf_hashes, buffer);
   }

   buffer += "}\n\n"sv;
}

void read_barrier(Ucfb_reader_strict<"BARR"_mn> barrier, std::string& buffer)
{
   auto info = barrier.read_child_strict<"INFO"_mn>();

   const auto name = info.read_child_strict<"NAME"_mn>().read_string();
   const auto transform = info.read_child_strict<"XFRM"_mn>().read_trivial<Transform>();
   const auto size = info.read_child_strict<"SIZE"_mn>().read_trivial<glm::vec3>();
   const auto flags = info.read_child_strict<"FLAG"_mn>().read_trivial<std::uint32_t>();

   buffer += "Barrier(\""sv;
   buffer += name;
   buffer += "\")\n{\n"sv;

   for (const auto& corner : get_barrier_corners(transform, size)) {
      write_key_value(true, "Corner"sv, corner, buffer);
   }

   write_key_value(true, "Flag"sv, flags, buffer);
   buffer += "}\n\n"sv;
}

void read_hint(Ucfb_reader_strict<"Hint"_mn> hint, const Swbf_fnv_hashes& swbf_hashes,
               std::string& buffer)
{
   auto info = hint.read_child_strict<"INFO"_mn>();

   const auto type = info.read_child_strict<"TYPE"_mn>().read_string();
   const auto name = info.read_child_strict<"NAME"_mn>().read_string();
   const auto transform = info.read_child_strict<"XFRM"_mn>().read_trivial<Transform>();

   buffer += "Hint(\""sv;
   buffer += name;
   buffer += "\", \""sv;
   buffer += type;
   buffer += "\")\n{\n"sv;

   const auto world_coords = convert_transform(transform);
   write_key_value(true, "Position"sv, world_coords.second, buffer);
   write_key_value(true, "Rotation"sv, world_coords.first, buffer);

   while (hint) {
      const auto property = hint.read_child_strict<"PROP"_mn>();

      read_property(property, swbf_hashes, buffer);
   }

   buffer += "}\n\n"sv;
}

void read_animation(Ucfb_reader_strict<"anim"_mn> animation, std::string& buffer)
{
   auto info = animation.read_child_strict<"INFO"_mn>();

   const auto name = info.read_string_unaligned();
   const auto length = info.read_trivial_unaligned<float>();
   const int loop = info.read_trivial_unaligned<std::uint8_t>();
   const int local_translation = info.read_trivial_unaligned<std::uint8_t>();

   buffer += fmt::format("Animation(\"{}\", {}, {}, {})\n{{\n"sv, name, length, loop,
                         local_translation);

   while (animation) {
      auto key = animation.read_child();

      Animation_key key_info;
      key_info.time = key.read_trivial_unaligned<float>();
      key_info.data = key.read_trivial_unaligned<std::array<float, 3>>();
      key_info.type = key.read_trivial_unaligned<std::uint8_t>();
      key_info.spline_data = key.read_trivial_unaligned<std::array<float, 6>>();

      if (key.magic_number() == "ROTK"_mn) {
         for (auto& v : key_info.data) v = glm::degrees(v);
         for (auto& v : key_info.spline_data) v = glm::degrees(v);

         write_animation_key("AddRotationKey"sv, key_info, buffer);
      }
      else if (key.magic_number() == "POSK"_mn) {
         write_animation_key("AddPositionKey"sv, key_info, buffer);
      }
   }

   buffer += "}\n\n"sv;
}

void read_animation_group(Ucfb_reader_strict<"anmg"_mn> anim_group, std::string& buffer)
{
   auto info = anim_group.read_child_strict<"INFO"_mn>();

   const auto name = info.read_string_unaligned();
   const int default_on = info.read_trivial_unaligned<std::uint8_t>();
   const int stop_when_controlled = info.read_trivial_unaligned<std::uint8_t>();

   buffer += fmt::format("AnimationGroup(\"{}\", {}, {})\n{{\n"sv, name, default_on,
                         stop_when_controlled);

   while (anim_group) {
      auto child = anim_group.read_child();

      switch (child.magic_number()) {
      case "ANIM"_mn: {
         const auto animation_name = child.read_string_unaligned();
         const auto object_name = child.read_string_unaligned();

         buffer +=
            fmt::format("\tAnimation(\"{}\", \"{}\");\n"sv, animation_name, object_name);
         break;
      }
      case "NOHI"_mn: {
         buffer += "\tDisableHierarchies();\n"sv;
         break;
      }
      }
   }

   buffer += "}\n\n"sv;
}

void read_animation_hierarchy(Ucfb_reader_strict<"anmh"_mn> anim_hierarchy,
                              std::string& buffer)
{
   auto info = anim_hierarchy.read_child_strict<"INFO"_mn>();

   const auto string_count = info.read_trivial_unaligned<std::uint8_t>();

   std::vector<std::string_view> strings;
   strings.reserve(string_count);

   for (std::size_t i = 0; i < string_count; ++i) {
      strings.emplace_back(info.read_string_unaligned());
   }

   buffer += "Hierarchy(\""sv;
   buffer += strings[0];
   buffer += "\")\n{\n"sv;

   for (std::size_t i = 1; i < strings.size(); ++i) {
      buffer += "\tObj(\""sv;
      buffer += strings[i];
      buffer += "\");\n"sv;
   }

   buffer += "}\n\n"sv;
}

void read_instance(Ucfb_reader_strict<"inst"_mn> instance,
                   const Swbf_fnv_hashes& swbf_hashes, std::string& buffer)
{
   auto info = instance.read_child_strict<"INFO"_mn>();

   const auto type = info.read_child_strict<"TYPE"_mn>().read_string();
   const auto name = info.read_child_strict<"NAME"_mn>().read_string();
   const auto transform = info.read_child_strict<"XFRM"_mn>().read_trivial<Transform>();

   buffer += "Object(\""sv;
   buffer += name;
   buffer += "\", \""sv;
   buffer += type;
   buffer += "\", 1)\n{\n"sv;

   const auto world_coords = convert_transform(transform);
   write_key_value(true, "ChildRotation"sv, world_coords.first, buffer);
   write_key_value(true, "ChildPosition"sv, world_coords.second, buffer);

   while (instance) {
      auto property = instance.read_child_strict<"PROP"_mn>();

      read_property(property, swbf_hashes, buffer, [](std::uint32_t hash) {
         return (hash != "Team"_fnv && hash != "Layer"_fnv);
      });
   }

   buffer += "}\n\n"sv;
}

void process_region_entries(std::vector<Ucfb_reader_strict<"regn"_mn>> regions,
                            std::string_view name, File_saver& file_saver,
                            const Swbf_fnv_hashes& swbf_hashes)
{
   std::string buffer;
   buffer.reserve(256 * regions.size());
   buffer += "Version(1);\n"sv;

   write_key_value(false, "RegionCount"sv, regions.size(), buffer);
   buffer += '\n';

   for (const auto& region : regions) {
      read_region(region, swbf_hashes, buffer);
   }

   file_saver.save_file(buffer, "world"sv, name, ".rgn"sv);
}

void process_instance_entries(std::vector<Ucfb_reader_strict<"inst"_mn>> instances,
                              const std::string name, const std::string terrain_name,
                              const std::string sky_name, File_saver& file_saver,
                              const Swbf_fnv_hashes& swbf_hashes)
{
   std::string buffer;
   buffer.reserve((world_header.length() + 256) + 256 * instances.size());

   buffer += world_header;
   buffer += '\n';

   if (!terrain_name.empty())
      write_key_value(false, true, "TerrainName"sv, terrain_name + ".ter"s, buffer);
   if (!sky_name.empty())
      write_key_value(false, true, "SkyName"sv, sky_name + ".sky"s, buffer);

   write_key_value(false, true, "LightName"sv, name + ".lgt"s, buffer);
   buffer += '\n';

   for (const auto& instance : instances) {
      read_instance(instance, swbf_hashes, buffer);
   }

   std::string_view extension = ".wld"sv;

   if (terrain_name.empty() || sky_name.empty()) extension = ".lyr"sv;

   file_saver.save_file(buffer, "world"sv, name, extension);
}

void process_barrier_entries(std::vector<Ucfb_reader_strict<"BARR"_mn>> barriers,
                             std::string_view name, File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(256 * barriers.size());

   write_key_value(false, "BarrierCount"sv, barriers.size(), buffer);
   buffer += '\n';

   for (const auto& barrier : barriers) {
      read_barrier(barrier, buffer);
   }

   file_saver.save_file(buffer, "world"sv, name, ".bar"sv);
}

void process_hint_entries(std::vector<Ucfb_reader_strict<"Hint"_mn>> hints,
                          std::string_view name, File_saver& file_saver,
                          const Swbf_fnv_hashes& swbf_hashes)
{
   std::string buffer;
   buffer.reserve(256 * hints.size());

   for (const auto& hint : hints) {
      read_hint(hint, swbf_hashes, buffer);
   }

   file_saver.save_file(buffer, "world"sv, name, ".hnt"sv);
}

void process_animation_entries(std::vector<Ucfb_reader> entries, std::string_view name,
                               File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(512 * entries.size());

   for (const auto& entry : entries) {
      if (entry.magic_number() == "anim"_mn) {
         read_animation(Ucfb_reader_strict<"anim"_mn>{entry}, buffer);
      }
      else if (entry.magic_number() == "anmg"_mn) {
         read_animation_group(Ucfb_reader_strict<"anmg"_mn>{entry}, buffer);
      }
      else if (entry.magic_number() == "anmh"_mn) {
         read_animation_hierarchy(Ucfb_reader_strict<"anmh"_mn>{entry}, buffer);
      }
   }

   file_saver.save_file(buffer, "world"sv, name, ".anm"sv);
}
}

void handle_world(Ucfb_reader world, File_saver& file_saver,
                  const Swbf_fnv_hashes& swbf_hashes)
{
   const auto name = world.read_child_strict<"NAME"_mn>().read_string();

   std::string_view terrain_name;

   auto terrain_name_reader = world.read_child_strict_optional<"TNAM"_mn>();
   if (terrain_name_reader) terrain_name = terrain_name_reader->read_string();

   std::string_view sky_name;

   auto sky_name_reader = world.read_child_strict_optional<"SNAM"_mn>();
   if (sky_name_reader) sky_name = sky_name_reader->read_string();

   std::vector<Ucfb_reader_strict<"regn"_mn>> region_entries;
   std::vector<Ucfb_reader_strict<"inst"_mn>> instance_entries;
   std::vector<Ucfb_reader_strict<"BARR"_mn>> barrier_entries;
   std::vector<Ucfb_reader_strict<"Hint"_mn>> hint_entries;
   std::vector<Ucfb_reader> animation_entries;

   while (world) {
      auto child = world.read_child();

      if (child.magic_number() == "regn"_mn) {
         region_entries.emplace_back(Ucfb_reader_strict<"regn"_mn>{child});
      }
      else if (child.magic_number() == "inst"_mn) {
         instance_entries.emplace_back(Ucfb_reader_strict<"inst"_mn>{child});
      }
      else if (child.magic_number() == "BARR"_mn) {
         barrier_entries.emplace_back(Ucfb_reader_strict<"BARR"_mn>{child});
      }
      else if (child.magic_number() == "Hint"_mn) {
         hint_entries.emplace_back(Ucfb_reader_strict<"Hint"_mn>{child});
      }
      else if (child.magic_number() == "anim"_mn) {
         animation_entries.emplace_back(child);
      }
      else if (child.magic_number() == "anmg"_mn) {
         animation_entries.emplace_back(child);
      }
      else if (child.magic_number() == "anmh"_mn) {
         animation_entries.emplace_back(child);
      }
   }

   tbb::task_group tasks;

   tasks.run(
      [region_entries{std::move(region_entries)}, name, &file_saver, &swbf_hashes] {
         process_region_entries(region_entries, name, file_saver, swbf_hashes);
      });

   tasks.run([instance_entries{std::move(instance_entries)}, name, terrain_name, sky_name,
              &file_saver, &swbf_hashes] {
      process_instance_entries(instance_entries, std::string{name},
                               std::string{terrain_name}, std::string{sky_name},
                               file_saver, swbf_hashes);
   });

   tasks.run([barrier_entries{std::move(barrier_entries)}, name, &file_saver] {
      process_barrier_entries(barrier_entries, name, file_saver);
   });

   tasks.run([hint_entries{std::move(hint_entries)}, name, &file_saver, &swbf_hashes] {
      process_hint_entries(hint_entries, name, file_saver, swbf_hashes);
   });

   if (!animation_entries.empty()) {
      tasks.run([animation_entries{std::move(animation_entries)}, name, &file_saver] {
         process_animation_entries(animation_entries, name, file_saver);
      });
   }

   tasks.wait();
}
