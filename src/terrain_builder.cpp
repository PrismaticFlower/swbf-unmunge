
#include "terrain_builder.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"

#include <gsl/gsl>

using namespace std::literals;

namespace {
}

Terrain_builder::Terrain_builder(const float grid_unit_size, const float height_scale,
                                 const std::uint16_t grid_size)
   : _grid_unit_size{grid_unit_size}, _height_granularity{height_scale},
     _grid_size{grid_size}, _heightmap(grid_size * grid_size, 0i16),
     _colourmap(grid_size * grid_size, 0xffffffffui32),
     _texturemap(grid_size * grid_size, Texture_values{}),
     _patch_infomap((grid_size / 4) * (grid_size / 4), {Render_types::normal, 0})
{
}

void Terrain_builder::set_textures(const std::array<std::string, max_textures>& textures)
{
   for (std::size_t i = 0; i < max_textures; ++i) {
      if (!textures[i].empty()) {
         copy_to_cstring(textures[i] + ".tga"s, _textures[i].diffuse.data(),
                         _textures[i].diffuse.size());
      }
   }
}

void Terrain_builder::set_detail_texture(std::string_view texture)
{
   std::string full_tex_name{texture};
   full_tex_name += ".tga"_sv;

   for (std::size_t i = 0; i < max_textures; ++i) {
      copy_to_cstring(full_tex_name, _textures[i].detail.data(),
                      _textures[i].detail.size());
   }
}

void Terrain_builder::set_texture_options(
   const std::array<float, max_textures>& scales,
   const std::array<std::uint8_t, max_textures>& axises,
   const std::array<float, max_textures>& rotations)
{
   for (std::size_t i = 0; i < max_textures; ++i) {
      _texture_scales[i] = 1.0f / scales[i];
   }

   _texture_axises = axises;
   _texture_rotations = rotations;
}

void Terrain_builder::set_water_settings(const float height, glm::vec2 velocity,
                                         glm::vec2 repeat, std::uint32_t colour,
                                         std::string_view texture_name)
{
   auto& settings = _water_settings[1];

   settings.height = height;
   settings.u_velocity = velocity.x;
   settings.v_velocity = velocity.y;
   settings.u_repeat = repeat.x;
   settings.v_repeat = repeat.y;
   settings.colour = colour;

   if (!texture_name.empty()) {
      std::string full_tex_name{texture_name};
      full_tex_name += ".tga"_sv;

      copy_to_cstring(full_tex_name, settings.texture_name.data(),
                      settings.texture_name.size());
   }
}

void Terrain_builder::set_point_height(const Point point, const float height) noexcept
{
   const auto int16_height = static_cast<std::int16_t>(height / _height_granularity);

   _heightmap[lookup_point_index(point)] = int16_height;
}

void Terrain_builder::set_point_colour(const Point point,
                                       const std::uint32_t colour) noexcept
{
   _colourmap[lookup_point_index(point)] = (colour | 0xFF000000);
}

void Terrain_builder::set_point_texture(const Point point,
                                        const std::uint_fast8_t texture,
                                        const std::uint8_t value) noexcept
{
   Expects(texture < max_textures);

   _texturemap[lookup_point_index(point)][texture] = value;
}

void Terrain_builder::set_patch_water(const Point patch, const bool water)
{
   const auto index = lookup_patch_index(patch);

   auto& patch_info = _patch_infomap[index];

   patch_info.water_layer = water ? 1 : 0;
}

void Terrain_builder::save(std::string_view name, File_saver& file_saver) const
{
   constexpr auto header_size = 2821;

   std::string buffer;
   buffer.reserve(2821 + (_heightmap.size() * sizeof(std::int16_t)) +
                  ((_colourmap.size() * sizeof(std::uint32_t)) * 2) +
                  ((_texturemap.size() * sizeof(Texture_values))) +
                  ((_grid_size / 2) * (_grid_size / 2)) + // unknown map
                  ((_patch_infomap.size() * sizeof(Patch_info))));

   // magic number
   buffer += "TERR"_sv;

   // version number
   buffer += view_pod_as_string(22i32);

   // grid extent
   const auto extent = static_cast<std::int16_t>(_grid_size / 2);

   buffer += view_pod_as_string<std::int16_t>(-extent);
   buffer += view_pod_as_string<std::int16_t>(-extent);
   buffer += view_pod_as_string(extent);
   buffer += view_pod_as_string(extent);

   // unknown
   buffer += view_pod_as_string(164i32);

   // texture scales
   static_assert(sizeof(_texture_scales) == 64);
   buffer += view_pod_as_string(_texture_scales);

   // texture axises
   static_assert(sizeof(_texture_axises) == 16);
   buffer += view_pod_as_string(_texture_axises);

   // texture rotations
   static_assert(sizeof(_texture_rotations) == 64);
   buffer += view_pod_as_string(_texture_rotations);

   // height granularity
   buffer += view_pod_as_string(_height_granularity);

   // metere per grid unit
   buffer += view_pod_as_string(_grid_unit_size);

   // prelit
   buffer += view_pod_as_string(0i32);

   // world size
   buffer += view_pod_as_string(std::uint32_t{_grid_size});

   // grids per foliage
   buffer += view_pod_as_string(2i32);

   // unknown
   buffer += '\0';

   // texture names
   static_assert(sizeof(_textures) == 1024);
   buffer += view_pod_as_string(_textures);

   // water settings
   static_assert(sizeof(_water_settings) == 1088);
   buffer += view_pod_as_string(_water_settings);

   // decal textures
   buffer.append(32 * max_decal_textures, '\0');

   // decal tile count
   buffer += view_pod_as_string(0i32);

   // unknown decal options(?)
   buffer.append(8, '\0');

   // heightmap
   buffer += view_pod_span_as_string(gsl::make_span(_heightmap));

   // colourmap foreground
   buffer += view_pod_span_as_string(gsl::make_span(_colourmap));

   // colourmap background
   buffer.append(4 * _colourmap.size(), '\xff');

   // texturemap
   buffer += view_pod_span_as_string(gsl::make_span(_texturemap));

   // unknown map
   buffer.append((_grid_size / 2) * (_grid_size / 2), '\0');

   // patch infomap
   buffer += view_pod_span_as_string(gsl::make_span(_patch_infomap));

   file_saver.save_file(std::move(buffer), std::string{name} + ".ter"s, "world");
}

std::size_t Terrain_builder::lookup_point_index(Point point) const noexcept
{
   if (point[0] >= _grid_size) point[0] %= _grid_size;
   if (point[1] >= _grid_size) point[1] %= _grid_size;

   return point[0] + _grid_size * point[1];
}

std::size_t Terrain_builder::lookup_patch_index(Point patch) const noexcept
{
   const auto patch_grid_size = (_grid_size / 4);

   if (patch[0] >= patch_grid_size) patch[0] %= patch_grid_size;
   if (patch[1] >= patch_grid_size) patch[1] %= patch_grid_size;

   return patch[0] + patch_grid_size * patch[1];
}
