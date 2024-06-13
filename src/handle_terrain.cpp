
#include "app_options.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "terrain_builder.hpp"
#include "ucfb_reader.hpp"

#include <glm/glm.hpp>
#include <gsl/gsl>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace std::literals;

namespace {

struct Terrain_info {
   float grid_unit_size = 0.0f;
   float height_scale = 0.0f;
   float height_floor = 0.0f;
   float height_ceiling = 0.0f;
   std::uint16_t grid_length = 0;
   std::uint16_t patch_length = 0;
   std::uint16_t height_map_patch_size = 0;
   std::uint16_t texture_count = 0;
   std::uint16_t decal_textures_count = 0;
   std::uint16_t decal_tiles_count = 0;
};

static_assert(sizeof(Terrain_info) == 28);

struct Height_patch_info {
   std::uint8_t shift = 0;
   std::uint8_t cutter_flags = 0;
   std::int16_t add = 0;
   std::int16_t min = 0;
   std::int16_t max = 0;
};

static_assert(sizeof(Height_patch_info) == 8);

struct Stock_terrain_vertex_pc {
   glm::i16vec3 position;
   glm::uint16 texture_weight;
   glm::uint32 normal;
   glm::uint32 color;
};

static_assert(sizeof(Stock_terrain_vertex_pc) == 16);

struct Attribute_maps {
   std::vector<std::uint32_t> light_map;
   std::vector<std::array<std::uint8_t, 16>> weight_map;
};

auto read_hexp(Ucfb_reader_strict<"HEXP"_mn> hexp, Terrain_info info)
   -> std::vector<Height_patch_info>
{
   const std::size_t height_patches_length =
      info.grid_length / info.height_map_patch_size;
   const std::size_t height_patches_count = height_patches_length * height_patches_length;

   std::vector<Height_patch_info> height_patch_info{height_patches_count};

   hexp.read_array_to_span_unaligned(height_patches_count, gsl::span{height_patch_info});

   return height_patch_info;
}

auto read_hgt8(Ucfb_reader_strict<"HGT8"_mn> hgt8, Terrain_info info,
               gsl::span<const Height_patch_info> height_patch_info)
   -> std::vector<std::int16_t>
{
   std::vector<std::uint8_t> compressed_heightmap;
   compressed_heightmap.reserve(info.grid_length * info.grid_length);

   while (hgt8) {
      const auto sequence_desc = hgt8.read_trivial_unaligned<std::uint8_t>();
      const std::size_t sequence_count = sequence_desc & 0x7fu;
      const bool sequence_duplicate = (sequence_desc & 0x80) == 0x80;

      if (sequence_duplicate) {
         const std::uint8_t entry =
            hgt8 ? hgt8.read_trivial_unaligned<std::uint8_t>() : 0;

         for (std::size_t i = 0; i <= sequence_count; ++i) {
            compressed_heightmap.push_back(entry);
         }
      }
      else {
         for (std::size_t i = 0; i <= sequence_count; ++i) {
            const std::uint8_t entry =
               hgt8 ? hgt8.read_trivial_unaligned<std::uint8_t>() : 0;

            compressed_heightmap.push_back(entry);
         }
      }
   }

   compressed_heightmap.resize(info.grid_length * info.grid_length);

   const std::size_t height_patches_length =
      info.grid_length / info.height_map_patch_size;
   const std::size_t patch_length = info.height_map_patch_size;
   const std::size_t patch_length_mask = patch_length - 1u;
   const std::size_t patch_size = patch_length * patch_length;

   std::vector<std::int16_t> heightmap;
   heightmap.resize(info.grid_length * info.grid_length);

   for (std::size_t z = 0; z < info.grid_length; ++z) {
      for (std::size_t x = 0; x < info.grid_length; ++x) {
         const std::size_t patch_x = x / patch_length;
         const std::size_t patch_z = z / patch_length;
         const std::size_t patch_index = patch_z * height_patches_length + patch_x;

         const std::size_t local_x = x & patch_length_mask;
         const std::size_t local_z = z & patch_length_mask;
         const std::size_t local_index = local_z * patch_length + local_x;

         const std::size_t compressed_height_index =
            local_index + (patch_index * patch_size);

         if (compressed_height_index >= compressed_heightmap.size()) continue;

         const auto compressed_height = compressed_heightmap[compressed_height_index];
         const auto patch = height_patch_info[patch_index];

         const std::int16_t height = (compressed_height << patch.shift) + patch.add;

         heightmap[z * info.grid_length + x] = height;
      }
   }

   return heightmap;
}

auto read_cutr(Ucfb_reader_strict<"CUTR"_mn> cutr) -> Terrain_cut
{
   Terrain_cut cut;

   while (cutr) {
      auto child = cutr.read_child();

      if (const auto mn = child.magic_number(); mn == "INFO"_mn) {
         const auto [plane_count, bounds_radius, bounds_centre] =
            child.read_multi_unaligned<std::uint32_t, float, glm::vec3>();

         cut.bounds_radius = bounds_radius;
         cut.bounds_centre = bounds_centre;
         cut.planes.resize(plane_count);
      }
      else if (mn == "PLNS"_mn) {
         for (auto& plane : cut.planes) plane = child.read_trivial_unaligned<glm::vec4>();
      }
   }

   return cut;
}

auto read_cuts(Ucfb_reader_strict<"CUTS"_mn> cuts) -> std::vector<Terrain_cut>
{
   std::vector<Terrain_cut> result;

   while (cuts) {
      auto child = cuts.read_child();

      if (const auto mn = child.magic_number(); mn == "INFO"_mn) {
         auto count = child.read_trivial_unaligned<std::uint32_t>();

         result.reserve(count);
      }
      else if (mn == "CUTR"_mn) {
         result.push_back(read_cutr(Ucfb_reader_strict<"CUTR"_mn>{child}));
      }
   }

   return result;
}

auto read_ptch_info(Ucfb_reader_strict<"INFO"_mn> info) -> std::array<std::uint8_t, 3>
{
   std::array<std::uint8_t, 3> result{};

   std::int8_t texture_count = info.read_trivial_unaligned<std::uint8_t>();

   for (std::ptrdiff_t i = 0; i < texture_count; ++i) {
      std::uint8_t texture_index = info.read_trivial_unaligned<std::uint8_t>();

      if (i >= std::ssize(result)) continue;

      result[i] = texture_index;
   }

   for (std::ptrdiff_t i = 2; i > (texture_count - 1); --i) {
      result[i] = result[0];
   }

   return result;
}

auto read_pchs(Ucfb_reader_strict<"PCHS"_mn> pchs, Terrain_info info) -> Attribute_maps
{
   const std::size_t patches_length = info.grid_length / info.patch_length;

   Attribute_maps attributes;

   attributes.light_map.resize(info.grid_length * info.grid_length, 0xff'00'ff'00u);
   attributes.weight_map.resize(info.grid_length * info.grid_length,
                                std::array<std::uint8_t, 16>{0xff});

   (void)pchs.read_child_strict<"COMN"_mn>();

   const std::size_t patch_points = info.patch_length + 1;

   for (std::size_t patch_z = 0; patch_z < patches_length; ++patch_z) {
      for (std::size_t patch_x = 0; patch_x < patches_length; ++patch_x) {
         auto ptch = pchs.read_child_strict<"PTCH"_mn>();

         const std::array<std::uint8_t, 3> patch_textures =
            read_ptch_info(ptch.read_child_strict<"INFO"_mn>());

         while (ptch) {
            auto vbuf = ptch.read_child();

            if (vbuf.magic_number() != "VBUF"_mn) continue;

            const auto [count, stride, flags] =
               vbuf.read_multi<std::uint32_t, std::uint32_t, std::uint32_t>();

            if (stride != 16) continue;

            for (std::size_t v = 0; v < count; ++v) {
               auto vertex = vbuf.read_trivial<Stock_terrain_vertex_pc>();

               const std::size_t x =
                  ((patch_x * info.patch_length) +
                   static_cast<std::size_t>(((vertex.position.x + 0x8000) / 65535.0) *
                                            patch_points)) %
                  info.grid_length;
               const std::size_t z =
                  ((patch_z * info.patch_length) +
                   static_cast<std::size_t>(((vertex.position.z + 0x8000) / 65535.0) *
                                            patch_points)) %
                  info.grid_length;

               // TODO: Does Z need to be offset by -1?;

               const auto texture_weight_0 =
                  static_cast<std::uint8_t>((vertex.color >> 24u) & 0xffu);
               const auto texture_weight_1 =
                  static_cast<std::uint8_t>((vertex.normal >> 24u) & 0xffu);
               const auto texture_weight_2 =
                  static_cast<std::uint8_t>(vertex.texture_weight & 0xffu);

               attributes.light_map[z * info.grid_length + x] =
                  vertex.color | 0xff'00'00'00u;

               std::array<std::uint8_t, 16>& weights =
                  attributes.weight_map[z * info.grid_length + x];

               if (patch_textures[0] < weights.size()) {
                  weights[patch_textures[0]] = texture_weight_0;
               }
               if (patch_textures[1] < weights.size()) {
                  weights[patch_textures[1]] = texture_weight_1;
               }
               if (patch_textures[2] < weights.size()) {
                  weights[patch_textures[2]] = texture_weight_2;
               }
            }

            break;
         }
      }
   }

   return attributes;
}

auto read_folg(Ucfb_reader_strict<"FOLG"_mn> folg) -> std::vector<std::uint8_t>
{
   std::vector<std::uint8_t> result;

   const auto size = folg.read_trivial_unaligned<std::uint32_t>();

   result.resize(size);

   folg.read_array_to_span_unaligned(size, gsl::span{result});

   return result;
}

}

void handle_terrain(Ucfb_reader terrain, Game_version output_version,
                    File_saver& file_saver)
{
   std::string name;
   std::optional<Terrain_info> info;

   std::array<std::string, 16> textures;
   std::string detail_texture;

   std::array<float, 16> texture_scales;
   std::ranges::fill(texture_scales, 1.0f);
   std::array<Terrain_texture_axis, 16> texture_axes{};
   std::array<float, 16> texture_rotations{};

   std::vector<Height_patch_info> height_patch_info;

   std::vector<std::int16_t> height_map;
   Attribute_maps attribute_maps;
   std::vector<std::uint8_t> foliage_map;

   std::vector<Terrain_cut> terrain_cuts;

   while (terrain) {
      auto child = terrain.read_child();

      if (const auto mn = child.magic_number(); mn == "NAME"_mn) {
         name = child.read_string();
      }
      else if (mn == "INFO"_mn) {
         info = child.read_trivial<Terrain_info>();
      }
      else if (mn == "LTEX"_mn) {
         if (!info) continue;

         for (std::size_t i = 0; i < std::min(info->texture_count, std::uint16_t{16});
              ++i) {
            textures[i] = child.read_string_unaligned();
         }
      }
      else if (mn == "DTLX"_mn) {
         detail_texture = child.read_string();
      }
      else if (mn == "SCAL"_mn) {
         if (!info) continue;

         child.read_array_to_span_unaligned(
            std::min(info->texture_count, std::uint16_t{16}),
            gsl::make_span(texture_scales));
      }
      else if (mn == "AXIS"_mn) {
         if (!info) continue;

         child.read_array_to_span_unaligned(
            std::min(info->texture_count, std::uint16_t{16}),
            gsl::make_span(texture_axes));
      }
      else if (mn == "ROTN"_mn) {
         if (!info) continue;

         child.read_array_to_span_unaligned(
            std::min(info->texture_count, std::uint16_t{16}),
            gsl::make_span(texture_rotations));
      }
      else if (mn == "HEXP"_mn) {
         if (!info) continue;

         height_patch_info = read_hexp(Ucfb_reader_strict<"HEXP"_mn>{child}, *info);
      }
      else if (mn == "HGT8"_mn) {
         if (!info || height_patch_info.empty()) continue;

         height_map =
            read_hgt8(Ucfb_reader_strict<"HGT8"_mn>{child}, *info, height_patch_info);
      }
      else if (mn == "CUTS"_mn) {
         terrain_cuts = read_cuts(Ucfb_reader_strict<"CUTS"_mn>{child});
      }
      else if (mn == "PCHS"_mn) {
         if (!info) continue;

         attribute_maps = read_pchs(Ucfb_reader_strict<"PCHS"_mn>{child}, *info);
      }
      else if (mn == "FOLG"_mn) {
         foliage_map = read_folg(Ucfb_reader_strict<"FOLG"_mn>{child});
      }
      else if (mn == "WATR"_mn) {
         // read_watr(Ucfb_reader_strict<"WATR"_mn>{child}); // TODO: Read water
      }
   }

   if (!info) return;

   Terrain_builder builder{info->grid_unit_size, info->height_scale, info->grid_length};

   // builder.set_munge_flags(munge_flags); // TODO: Munge flags
   builder.set_textures(textures);
   builder.set_texture_options(texture_scales, texture_axes, texture_rotations);
   builder.set_detail_texture(detail_texture);

   const std::size_t max_z = info->grid_length - 1;

   for (std::size_t z = 0; z < info->grid_length; ++z) {
      for (std::size_t x = 0; x < info->grid_length; ++x) {
         builder.set_point_height({x, z},
                                  height_map.at((max_z - z) * info->grid_length + x));
         builder.set_point_light(
            {x, z}, attribute_maps.light_map.at((max_z - z) * info->grid_length + x));

         const std::array texture_weight_map =
            attribute_maps.weight_map.at((max_z - z) * info->grid_length + x);

         for (std::uint8_t i = 0; i < Terrain_builder::max_textures; ++i) {
            builder.set_point_texture({x, z}, i, texture_weight_map[i]);
         }
      }
   }

   builder.save(output_version, name, file_saver);
}
