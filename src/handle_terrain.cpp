
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
      const bool sequence_duplicate = sequence_desc >> 7u;

      if (sequence_duplicate) {
         const auto entry = hgt8.read_trivial_unaligned<std::uint8_t>();

         for (std::size_t i = 0; i <= sequence_count; ++i) {
            compressed_heightmap.push_back(entry);
         }
      }
      else {
         for (std::size_t i = 0; i <= sequence_count; ++i) {
            const auto entry = hgt8.read_trivial_unaligned<std::uint8_t>();

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

         // read_pchs(Ucfb_reader_strict<"PCHS"_mn>{child}); // TODO: Read patches
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

   for (std::size_t z = 0; z < info->grid_length; ++z) {
      for (std::size_t x = 0; x < info->grid_length; ++x) {
         builder.set_point_height({x, z}, height_map[z * info->grid_length + x]);
      }
   }

   builder.save(output_version, name, file_saver);
}
