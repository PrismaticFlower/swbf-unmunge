#pragma once

#include "app_options.hpp"
#include "bit_flags.hpp"
#include "file_saver.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class Terrain_flags : char {
   munge_none = 0,
   munge_terrain = 1,
   munge_water = 2,
   munge_foliage = 4,
   munge_all = 7
};

enum class Terrain_texture_axis : std::uint8_t {
   xz,
   xy,
   yz,
   zx,
   yx,
   zy,
   negative_xz,
   negative_xy,
   negative_yz,
   negative_zx,
   negative_yx,
   negative_zy
};

struct Terrain_cut {
   float bounds_radius = 0.0f;
   glm::vec3 bounds_centre{};

   std::vector<glm::vec4> planes;
};

class Terrain_builder {
public:
   constexpr static auto max_textures = 16u;

   using Point = std::array<std::size_t, 2>;

   Terrain_builder(const float grid_unit_size, const float height_scale,
                   const std::uint16_t grid_size,
                   const std::uint32_t default_colour = 0xffffffffu);

   void set_textures(const std::array<std::string, max_textures>& textures);

   void set_detail_texture(std::string_view texture);

   void set_texture_options(const std::array<float, max_textures>& scales,
                            const std::array<Terrain_texture_axis, max_textures>& axises,
                            const std::array<float, max_textures>& rotations);

   void set_water_settings(const float height, glm::vec2 velocity, glm::vec2 repeat,
                           std::uint32_t colour, std::string_view texture_name);

   void set_point_height(const Point point, const std::int16_t height) noexcept;

   void set_point_colour(const Point point, const std::uint32_t colour) noexcept;

   void set_point_texture(const Point point, const std::uint_fast8_t texture,
                          const std::uint8_t value) noexcept;

   void set_patch_water(const Point patch, const bool water);

   void set_munge_flags(const Terrain_flags flags) noexcept;

   void save(Game_version version, std::string_view name, File_saver& file_saver) const;

private:
   using Texture_values = std::array<std::uint8_t, max_textures>;

   enum class Render_types : std::int16_t { none = 0, solid_colour = 4, normal = 15 };

   struct Terrain_texture_name {
      std::array<char, 32> diffuse;
      std::array<char, 32> detail;
   };

   struct Water_settings {
      float height;
      float unknown_0;
      float unknown_1;
      float unknown_2;
      float u_velocity;
      float v_velocity;
      float u_repeat;
      float v_repeat;
      std::uint32_t colour;
      std::array<char, 32> texture_name;
   };

   struct Patch_info {
      Render_types render_type;
      std::uint8_t water_layer;
      std::uint8_t unknown;
   };

   constexpr static auto max_water_layers = 16u;
   constexpr static auto max_decal_textures = 16u;

   std::size_t lookup_point_index(Point point) const noexcept;

   std::size_t lookup_patch_index(Point patch) const noexcept;

   const float _grid_unit_size = 8.0f;
   const float _height_granularity = 0.01f;
   const std::uint_fast16_t _grid_size = 128;
   Terrain_flags _terrain_flags = Terrain_flags::munge_all;

   std::vector<std::int16_t> _heightmap;
   std::vector<std::uint32_t> _colourmap;
   std::vector<Texture_values> _texturemap;
   std::vector<Patch_info> _patch_infomap;

   std::array<Terrain_texture_name, max_textures> _textures{};

   std::array<float, max_textures> _texture_scales{};
   std::array<Terrain_texture_axis, max_textures> _texture_axises{};
   std::array<float, max_textures> _texture_rotations{};

   std::array<Water_settings, max_water_layers> _water_settings{};
};

void save_void_terrain(Game_version version, std::string_view name,
                       File_saver& file_saver);
