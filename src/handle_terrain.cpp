
#include "app_options.hpp"
#include "byte.hpp"
#include "file_saver.hpp"
#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "math_helpers.hpp"
#include "string_helpers.hpp"
#include "terrain_builder.hpp"
#include "ucfb_reader.hpp"

#include "tbb/task_group.h"

#include <gsl/gsl>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace std::literals;

namespace {

enum class Vbuf_type : std::uint32_t {
   geometry = 290,
   texture = 20770,
   texture_extra = 130
};

struct Terrain_info {
   float grid_unit_size;
   float height_scale;
   float height_floor;
   float height_ceiling;
   std::uint16_t grid_size;
   std::uint16_t height_patches;
   std::uint16_t texture_patches;
   std::uint16_t texture_count;
   std::uint16_t max_texture_layers;
   std::uint16_t unknown;
};

static_assert(std::is_pod_v<Terrain_info>);
static_assert(sizeof(Terrain_info) == 28);

struct Vbuf_info {
   std::uint32_t element_count;
   std::uint32_t element_size;
   Vbuf_type element_type;
};

static_assert(std::is_pod_v<Vbuf_info>);
static_assert(sizeof(Vbuf_info) == 12);

struct Terrain_vbuf_entry {
   pod::Vec3 position;
   pod::Vec3 normal;

   std::uint32_t colour;
};

static_assert(std::is_pod_v<Terrain_vbuf_entry>);
static_assert(sizeof(Terrain_vbuf_entry) == 28);

struct Texture_vbuf_entry {
   std::uint16_t x;
   std::uint16_t y;
   std::uint16_t z;
   std::uint16_t unknown_0;

   std::uint8_t unknown_1;
   std::uint8_t texture_value_0;
   std::uint8_t unknown_2;
   std::uint8_t texture_value_1;

   std::uint32_t colour;
};

static_assert(std::is_pod_v<Texture_vbuf_entry>);
static_assert(sizeof(Texture_vbuf_entry) == 16);

struct Texture_vbuf_extra_entry {
   std::uint32_t x;
   std::uint32_t y;
   std::uint32_t z;

   Byte unknown_0[3];
   std::uint8_t texture_value;
};

static_assert(std::is_pod_v<Texture_vbuf_extra_entry>);
static_assert(sizeof(Texture_vbuf_extra_entry) == 16);

const static std::array<std::array<int, 2>, 81> patch_index_table{
   {{0, 8}, {1, 8}, {2, 8}, {3, 8}, {4, 8}, {5, 8}, {6, 8}, {7, 8}, {8, 8},
    {0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}, {6, 7}, {7, 7}, {8, 7},
    {0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {5, 6}, {6, 6}, {7, 6}, {8, 6},
    {0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5}, {6, 5}, {7, 5}, {8, 5},
    {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4}, {7, 4}, {8, 4},
    {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3}, {7, 3}, {8, 3},
    {0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2}, {7, 2}, {8, 2},
    {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1},
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}}};

auto create_patches_index_table(std::uint16_t grid_size)
   -> std::vector<std::array<int, 2>>
{
   constexpr auto patch_factor = 64;

   std::vector<std::array<int, 2>> index_table;
   index_table.resize(grid_size * grid_size / patch_factor);

   const auto index_length = static_cast<int>(std::sqrt(index_table.size()));

   for (auto x = 0; x < index_length; ++x) {
      for (auto y = 0; y < index_length; ++y) {
         index_table[x + index_length * y] = {x * 8, ((index_length - y) - 1) * 8};
      }
   }

   return index_table;
}

std::uint32_t pack_rgba_colour(std::array<std::uint8_t, 4> colour)
{
   std::uint32_t result = 0;

   result |= (colour[0] << 0);
   result |= (colour[1] << 8);
   result |= (colour[2] << 16);
   result |= (colour[3] << 24);

   return result;
}

auto read_texture_names(Ucfb_reader_strict<"LTEX"_mn> textures, std::size_t texture_count)
   -> std::array<std::string, Terrain_builder::max_textures>
{
   Expects(texture_count <= Terrain_builder::max_textures);

   std::array<std::string, Terrain_builder::max_textures> texture_names;

   for (std::size_t i = 0; i < texture_count; ++i) {
      texture_names[i] = textures.read_string_unaligned();
   }

   return texture_names;
}

template<typename Type, Magic_number magic_number>
auto read_texture_options(Ucfb_reader_strict<magic_number> options)
   -> std::array<Type, Terrain_builder::max_textures>
{
   return options.read_trivial<std::array<Type, Terrain_builder::max_textures>>();
}

void read_vbuf_elements(const std::array<Terrain_vbuf_entry, 81>& elements,
                        std::array<int, 2> patch_offset, Terrain_builder& builder)
{
   static_assert(sizeof(std::array<Terrain_vbuf_entry, 81>) ==
                 sizeof(Terrain_vbuf_entry) * 81);

   for (std::size_t i = 0; i < elements.size(); ++i) {
      const auto element_offset = patch_index_table[i];

      const std::array<std::uint16_t, 2> offset = {
         static_cast<std::uint16_t>(patch_offset[0] + element_offset[0]),
         static_cast<std::uint16_t>(patch_offset[1] + element_offset[1])};

      builder.set_point_height(offset, elements[i].position.y);
      builder.set_point_colour(offset, elements[i].colour | 0xFF000000);
   }
}

void read_vbuf_elements(const std::array<Texture_vbuf_entry, 81>& elements,
                        std::array<int, 2> patch_offset, std::uint_fast8_t& texture_index,
                        Terrain_builder& builder)
{
   static_assert(sizeof(std::array<Texture_vbuf_entry, 81>) ==
                 sizeof(Texture_vbuf_entry) * 81);

   for (std::size_t i = 0; i < elements.size(); ++i) {
      const auto element_offset = patch_index_table[i];

      const std::array<std::uint16_t, 2> offset = {
         static_cast<std::uint16_t>(patch_offset[0] + element_offset[0]),
         static_cast<std::uint16_t>(patch_offset[1] + element_offset[1])};

      builder.set_point_texture(offset, texture_index, elements[i].texture_value_0);
      builder.set_point_texture(offset, texture_index + 1, elements[i].texture_value_1);
   }

   texture_index += 2;
}

void read_vbuf_elements(const std::array<Texture_vbuf_extra_entry, 81>& elements,
                        std::array<int, 2> patch_offset, std::uint_fast8_t& texture_index,
                        Terrain_builder& builder)
{
   static_assert(sizeof(std::array<Texture_vbuf_extra_entry, 81>) ==
                 sizeof(Texture_vbuf_extra_entry) * 81);

   for (std::size_t i = 0; i < elements.size(); ++i) {
      const auto element_offset = patch_index_table[i];

      const std::array<std::uint16_t, 2> offset = {
         static_cast<std::uint16_t>(patch_offset[0] + element_offset[0]),
         static_cast<std::uint16_t>(patch_offset[1] + element_offset[1])};

      builder.set_point_texture(offset, texture_index, elements[i].texture_value);
   }

   ++texture_index;
}

void read_vbuf(Ucfb_reader_strict<"VBUF"_mn> vbuf, std::array<int, 2> patch_offset,
               std::uint_fast8_t& texture_index, Terrain_builder& builder)
{
   const auto info = vbuf.read_trivial<Vbuf_info>();

   if (info.element_count != 81) {
      // throw std::runtime_error{"Badly sized VBUF encountered in terrain."};
      return;
   }

   if (info.element_size != 28 && info.element_size != 16) {
      throw std::runtime_error{"Badly sized VBUF element encountered in terrain."};
   }

   if (info.element_type == Vbuf_type::texture_extra) {
      read_vbuf_elements(vbuf.read_trivial<std::array<Texture_vbuf_extra_entry, 81>>(),
                         patch_offset, texture_index, builder);
   }
   else if (info.element_type == Vbuf_type::texture) {
      read_vbuf_elements(vbuf.read_trivial<std::array<Texture_vbuf_entry, 81>>(),
                         patch_offset, texture_index, builder);
   }
   else if (info.element_type == Vbuf_type::geometry) {
      read_vbuf_elements(vbuf.read_trivial<std::array<Terrain_vbuf_entry, 81>>(),
                         patch_offset, builder);
   }
   else {
      throw std::runtime_error{"Unknown VBUF type encountered in terrain."};
   }
}

void read_patch(Ucfb_reader_strict<"PTCH"_mn> patch, std::array<int, 2> patch_offset,
                Terrain_builder& builder)
{
   patch.read_child_strict<"INFO"_mn>();

   std::uint_fast8_t texture_index = 0;

   while (patch) {
      const auto child = patch.read_child();

      if (child.magic_number() == "VBUF"_mn) {
         read_vbuf(Ucfb_reader_strict<"VBUF"_mn>{child}, patch_offset, texture_index,
                   builder);
      }
   }
}

void read_patches(Ucfb_reader_strict<"PCHS"_mn> patches, Terrain_info terrain_info,
                  Terrain_builder& builder)
{
   const auto index_table = create_patches_index_table(terrain_info.grid_size);

   patches.read_child_strict<"COMN"_mn>();

   for (std::size_t i = 0; i < index_table.size(); ++i) {
      read_patch(patches.read_child_strict<"PTCH"_mn>(), index_table[i], builder);
   }
}

void read_water_layer(Ucfb_reader_strict<"LAYR"_mn> layer, Terrain_builder& builder)
{

   const auto texture = layer.read_string_unaligned();
   layer.consume_unaligned(4);
   const auto height = layer.read_trivial_unaligned<float>();
   const auto velocity = layer.read_trivial_unaligned<pod::Vec2>();
   const auto repeat = layer.read_trivial_unaligned<pod::Vec2>();
   const auto colour =
      pack_rgba_colour(layer.read_trivial_unaligned<std::array<std::uint8_t, 4>>());

   builder.set_water_settings(height, velocity, {repeat.x, -repeat.y}, colour, texture);
}

void read_water_map(Ucfb_reader_strict<"WMAP"_mn> wmap, Terrain_info terrain_info,
                    Terrain_builder& builder)
{
   const auto watermap = wmap.read_array<std::uint8_t>(wmap.size());

   const auto water_map_length =
      static_cast<std::uint16_t>(std::floor(std::sqrt(watermap.size())));

   const std::uint16_t patches_length = terrain_info.grid_size / 4ui16;

   for (std::uint16_t x = 0; x < water_map_length; ++x) {
      for (std::uint16_t y = 0; y < water_map_length; ++y) {
         const std::array<std::uint16_t, 2> wmap_range = {0ui16, water_map_length};
         const std::array<std::uint16_t, 2> patch_range = {0ui16, patches_length};

         const std::uint16_t patch_x = range_convert(x, wmap_range, patch_range);
         const std::uint16_t patch_y = range_convert(y, wmap_range, patch_range);

         const auto index = (x + water_map_length * ((water_map_length - 1) - y));

         builder.set_patch_water({patch_x, patch_y}, (watermap[index] != 0));
      }
   }
}

void read_water(Ucfb_reader_strict<"WATR"_mn> water, Terrain_info terrain_info,
                Terrain_builder& builder)
{
   water.read_child_strict<"INFO"_mn>();

   read_water_layer(water.read_child_strict<"LAYR"_mn>(), builder);

   read_water_map(water.read_child_strict<"WMAP"_mn>(), terrain_info, builder);
}
}

void handle_terrain(Ucfb_reader terrain, Game_version output_version,
                    File_saver& file_saver)
{
   const auto name = terrain.read_child_strict<"NAME"_mn>().read_string();

   const auto info = terrain.read_child_strict<"INFO"_mn>().read_trivial<Terrain_info>();

   Terrain_builder builder{info.grid_unit_size, info.height_scale, info.grid_size};

   builder.set_textures(
      read_texture_names(terrain.read_child_strict<"LTEX"_mn>(), info.texture_count));

   terrain.read_child_strict<"DTEX"_mn>(); // Unused(?) detail texture array.

   builder.set_detail_texture(terrain.read_child_strict<"DTLX"_mn>().read_string());

   const auto scal = terrain.read_child_strict<"SCAL"_mn>();
   const auto axis = terrain.read_child_strict<"AXIS"_mn>();
   const auto rotn = terrain.read_child_strict<"ROTN"_mn>();

   builder.set_texture_options(read_texture_options<float>(scal),
                               read_texture_options<std::uint8_t>(axis),
                               read_texture_options<float>(rotn));

   while (terrain) {
      const auto child = terrain.read_child();

      if (child.magic_number() == "PCHS"_mn) {
         read_patches(Ucfb_reader_strict<"PCHS"_mn>{child}, info, builder);
      }
      else if (child.magic_number() == "WATR"_mn) {
         read_water(Ucfb_reader_strict<"WATR"_mn>{child}, info, builder);
      }
   }

   builder.save(output_version, name, file_saver);
}