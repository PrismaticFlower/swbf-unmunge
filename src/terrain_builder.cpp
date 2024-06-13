
#include "terrain_builder.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"

#include <gsl/gsl>

using namespace std::literals;

namespace {

template<typename T>
   requires(std::is_trivially_copyable_v<T>)
void write_span(std::ofstream& file, std::span<T> span) noexcept
{
   file.write(reinterpret_cast<const char*>(span.data()), span.size_bytes());
}

template<typename T>
   requires(std::is_trivially_copyable_v<T>)
void write_value(std::ofstream& file, const T& value) noexcept
{
   write_span(file, std::span{&value, 1});
}

}

Terrain_builder::Terrain_builder(const float grid_unit_size, const float height_scale,
                                 const std::uint16_t grid_size,
                                 const std::uint32_t default_colour)
   : _grid_unit_size{grid_unit_size}, _height_granularity{height_scale},
     _grid_size{grid_size}, _heightmap(grid_size * grid_size, 0i16),
     _lightmap(grid_size * grid_size, default_colour),
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
   full_tex_name += ".tga"sv;

   for (std::size_t i = 0; i < max_textures; ++i) {
      copy_to_cstring(full_tex_name, _textures[i].detail.data(),
                      _textures[i].detail.size());
   }
}

void Terrain_builder::set_texture_options(
   const std::array<float, max_textures>& scales,
   const std::array<Terrain_texture_axis, max_textures>& axises,
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
      full_tex_name += ".tga"sv;

      copy_to_cstring(full_tex_name, settings.texture_name.data(),
                      settings.texture_name.size());
   }
}

void Terrain_builder::set_point_height(const Point point,
                                       const std::int16_t height) noexcept
{
   _heightmap[lookup_point_index(point)] = height;
}

void Terrain_builder::set_point_light(const Point point,
                                      const std::uint32_t colour) noexcept
{
   _lightmap[lookup_point_index(point)] = (colour | 0xFF000000);
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

void Terrain_builder::set_munge_flags(const Terrain_flags flags) noexcept
{
   _terrain_flags = flags;
}

void Terrain_builder::save(Game_version version, std::string_view name,
                           File_saver& file_saver) const
{
   // const Clusters_info clusters_info = make_clusters_info();

   std::ofstream file =
      file_saver.open_save_file("world"sv, name, ".ter"sv, std::ios::binary);

   // magic number
   write_span(file, std::span{"TERR"sv});

   // version number
   write_value(file, version == Game_version::swbf_ii ? 22 : 21);

   // grid extent
   const auto extent = static_cast<std::int16_t>(_grid_size / 2);

   write_value<std::int16_t>(file, -extent);
   write_value<std::int16_t>(file, -extent);
   write_value<std::int16_t>(file, extent);
   write_value<std::int16_t>(file, extent);

   // unknown
   write_value<std::int32_t>(file, 164);

   // texture scales
   static_assert(sizeof(_texture_scales) == 64);
   write_value(file, _texture_scales);

   // texture axes
   static_assert(sizeof(_texture_axises) == 16);
   write_value(file, _texture_axises);

   // texture rotations
   static_assert(sizeof(_texture_rotations) == 64);
   write_value(file, _texture_rotations);

   // height granularity
   write_value(file, _height_granularity);

   // metere per grid unit
   write_value(file, _grid_unit_size);

   // prelit
   write_value<std::int32_t>(file, 1);

   // world size
   write_value<std::int32_t>(file, _grid_size);

   // grids per foliage
   write_value<std::int32_t>(file, 2);

   // munge flags
   if (version == Game_version::swbf_ii)
      write_value(file, static_cast<char>(_terrain_flags));

   // texture names
   static_assert(sizeof(_textures) == 1024);
   write_value(file, _textures);

   // water settings
   static_assert(sizeof(_water_settings) == 1088);
   write_value(file, _water_settings);

   // decal textures
   write_value(file, std::array<char, 32 * max_decal_textures>{});

   // decal tile count
   write_value<std::int32_t>(file, 0);

   // unknown decal options(?)
   write_value(file, std::array<char, 8>{});

   // heightmap
   write_span(file, std::span{_heightmap});

   // colourmap
   std::vector<std::uint32_t> colormap;
   colormap.resize(_lightmap.size(), 0xffffffff);

   write_span(file, std::span{colormap});

   // lightmap
   write_span(file, std::span{_lightmap});

   // texturemap
   write_span(file, std::span{_texturemap});

   // unknown map
   std::vector<std::uint8_t> unknown_map;
   unknown_map.resize((_grid_size / 2) * (_grid_size / 2), 0);

   write_span(file, std::span{unknown_map});

   // patch infomap
   write_span(file, std::span{_patch_infomap});
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

void save_void_terrain(Game_version version, std::string_view name,
                       File_saver& file_saver)
{
   Terrain_builder builder{8.0f, 0.01f, 128, 0x0};

   builder.set_munge_flags(Terrain_flags::munge_none);

   builder.save(version, name, file_saver);
}
