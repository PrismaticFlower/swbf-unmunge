
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"

#include "tbb/task_group.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace std::literals;

namespace {

#pragma pack(push, 1)

struct Terrain_vertex {
   float x;
   float y;
   float z;
   Byte unknown[12];

   std::uint32_t colour;
};

static_assert(std::is_standard_layout_v<Terrain_vertex>);
static_assert(sizeof(Terrain_vertex) == 28);

struct Terrain_texture_vertex {
   Byte unknown_0[11];
   std::uint8_t tex_val_1;
   Byte unknown_1[3];
   std::uint8_t tex_val_0;
};

static_assert(std::is_standard_layout_v<Terrain_texture_vertex>);
static_assert(sizeof(Terrain_texture_vertex) == 16);

struct Texture_names {
   Magic_number mn;
   std::uint32_t size;

   char names[];
};

static_assert(std::is_standard_layout_v<Texture_names>);
static_assert(sizeof(Texture_names) == 8);

struct Dtl_tex_name {
   Magic_number mn;
   std::uint32_t size;

   char str[];
};

static_assert(std::is_standard_layout_v<Dtl_tex_name>);
static_assert(sizeof(Dtl_tex_name) == 8);

template<typename Type>
struct Texture_metrics {
   Magic_number mn;
   std::uint32_t size;

   Type values[16];
};

static_assert(std::is_standard_layout_v<Texture_metrics<float>>);
static_assert(sizeof(Texture_metrics<float>) == 72);
static_assert(std::is_standard_layout_v<Texture_metrics<std::uint8_t>>);
static_assert(sizeof(Texture_metrics<std::uint8_t>) == 24);

struct Vbuf {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t element_count;
   std::uint32_t element_size;
   std::uint32_t flags; // flags???

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Vbuf>);
static_assert(sizeof(Vbuf) == 20);

struct Water_info {
   Magic_number mn;
   std::uint32_t size;

   Byte unknown_1[8];
   float water_height;
   Byte unknown_2[12];
};

static_assert(std::is_standard_layout_v<Water_info>);
static_assert(sizeof(Water_info) == 32);

struct Water_layer {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Water_layer>);
static_assert(sizeof(Water_layer) == 8);

struct Water_layer_info {
   Byte unknown_1[8];
   float u_vel;
   float v_vel; // Flipped in Zero Editor
   float u_rept;
   float v_rept;
   std::uint32_t colour;
};

static_assert(std::is_standard_layout_v<Water_layer_info>);
static_assert(sizeof(Water_layer_info) == 28);

struct Terrain_info {
   Magic_number mn;
   std::uint32_t size;

   float grid_size;
   float height_scale;
   float height_floor;
   float height_ceiling;

   std::uint16_t grid_length;
   std::uint16_t unknown_count_1;

   std::uint16_t unknown_count_2;
   std::uint16_t texture_count;

   std::uint16_t unknown[2];
};

static_assert(std::is_standard_layout_v<Terrain_info>);
static_assert(sizeof(Terrain_info) == 36);

struct Terrain_foliage {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t map_size;

   std::uint8_t data[];
};

static_assert(std::is_standard_layout_v<Terrain_foliage>);
static_assert(sizeof(Terrain_foliage) == 12);

struct Terrain_water {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Terrain_water>);
static_assert(sizeof(Terrain_water) == 8);

struct Terrain_patches {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t common_mn;
   std::uint32_t common_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Terrain_patches>);
static_assert(sizeof(Terrain_patches) == 16);

struct Terrain_patch {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t info_mn;
   std::uint32_t info_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Terrain_patch>);
static_assert(sizeof(Terrain_patch) == 16);

struct Ter_file_header {
   Magic_number mn = "TERR"_mn;
   std::uint32_t unknown_0 = 22;
   std::int16_t extents[4];
   std::uint32_t unknown_1 = 164;

   float tile_range[16];
   std::uint8_t tile_mapping[16];
   float tile_rotation[16];

   float height_scale;
   float grid_size;

   std::uint32_t unknown_2 = 1;
   std::uint32_t map_size;
   std::uint32_t unknown_3 = 2;

   struct Texture_name {
      char name[32] = {'\0'};
      char detail_name[32] = {'\0'};
   };

   static_assert(sizeof(Texture_name) == 64);

   Texture_name texture_names[16]{};
   Byte unknown_4[68]{};

   struct Water {
      std::uint8_t unknown_0 = 0;
      float height[2];
      Byte unknown[8];
      float u_vel;
      float v_vel;
      float u_repeat;
      float v_repeat;
      std::uint32_t colour;
      char texture_name[31];
   };

   static_assert(sizeof(Water) == 68);

   Water water[15]{};

   Byte unknown_5[525]{};
   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Ter_file_header>);
static_assert(sizeof(Ter_file_header) == 2821);

#pragma pack(pop)

std::vector<std::uint8_t> explode_foliage(const Terrain_foliage& folg)
{
   std::vector<std::uint8_t> exploded_folg;
   exploded_folg.resize(folg.map_size * 2);

   for (std::size_t i = 0; i < exploded_folg.size(); i += 2) {
      exploded_folg[i] = (folg.data[i / 2] >> 4) & 0x0F;
      exploded_folg[i + 1] = folg.data[i / 2] & 0x0F;
   }

   return exploded_folg;
}

class Terrain_builder {
public:
   Terrain_builder(std::string name, const Terrain_info& info) : Terrain_builder()
   {
      _name = std::move(name);
      _name += ".ter"_sv;

      _grid_size = info.grid_size;
      _height_scale = info.height_scale;
      _grid_length = info.grid_length;
      _texture_count = info.texture_count;

      _heightmap.resize(_grid_length * _grid_length);
      _colourmap.resize(_grid_length * _grid_length);
      _foliage_map.resize(_grid_length * _grid_length / 2);

      _patch_offsets.resize(_grid_length * _grid_length / 64);

      const std::size_t offsets_length =
         static_cast<std::size_t>(std::sqrt(_patch_offsets.size()));

      for (std::int32_t y = 0; y < offsets_length; ++y) {
         for (std::int32_t x = 0; x < offsets_length; ++x) {
            _patch_offsets[y * offsets_length + x] = {(x * _grid_size * 8.0f),
                                                      (y * _grid_size * 8.0f)};
         }
      }
   }

   void set_texture_scales(const Texture_metrics<float>& scales) noexcept
   {
      for (std::size_t i = 0; i < _texture_scales.size(); ++i) {
         _texture_scales[i] = 1.0f / scales.values[i];
      }
   }

   void set_texture_rotations(const Texture_metrics<float>& rotations) noexcept
   {
      for (std::size_t i = 0; i < _texture_rotations.size(); ++i) {
         _texture_rotations[i] = rotations.values[i];
      }
   }

   void set_texture_axises(const Texture_metrics<std::uint8_t>& axises) noexcept
   {
      for (std::size_t i = 0; i < _texture_axises.size(); ++i) {
         _texture_axises[i] = axises.values[i];
      }
   }

   void set_textures(const Texture_names& names) noexcept
   {
      std::uint32_t head = 0;
      const std::uint32_t end = names.size - 8;

      std::size_t index = 0;

      while (head < end) {
         _textures[index] = &names.names[head];
         head += static_cast<std::uint32_t>(_textures[index].length()) + 1;

         if (!_textures[index].empty()) _textures[index] += ".tga"_sv;

         ++index;
      }
   }

   void set_detail_texture(const Dtl_tex_name& name) noexcept
   {
      _detail_texture = name.str;
      if (!_detail_texture.empty()) _detail_texture += ".tga"_sv;
   }

   void set_foliage(const Terrain_foliage& folg)
   {
      const auto exploded_folg = explode_foliage(folg);
      const std::size_t folg_length =
         static_cast<std::size_t>(std::sqrt(exploded_folg.size()));

      const auto lookup_folg_info =
         [&exploded_folg, folg_length](std::size_t x, std::size_t y) -> std::uint8_t {
         constexpr std::size_t factor = 4;

         x /= factor;
         y /= factor;

         return exploded_folg[y * folg_length + x];
      };

      std::vector<std::uint8_t> foliage;
      foliage.resize(_grid_length * _grid_length);

      for (std::size_t y = 0; y < _grid_length; ++y) {
         for (std::size_t x = 0; x < _grid_length; ++x) {
            foliage[y * _grid_length + x] = lookup_folg_info(x, y);
         }
      }

      implode_foliage(foliage);
   }

   void set_water(const Terrain_water& watr)
   {
      std::uint32_t head = 0;
      const std::uint32_t end = watr.size;

      while (head < end) {
         const auto& chunk = view_type_as<chunks::Unknown>(watr.bytes[head]);

         if (chunk.mn == "INFO"_mn) {
            handle_water_info(view_type_as<Water_info>(watr.bytes[head]));
         }
         else if (chunk.mn == "LAYR"_mn) {
            handle_water_layer(view_type_as<Water_layer>(watr.bytes[head]));
         }

         head += chunk.size + 8;
         if (head % 4 != 0) head += (4 - (head % 4));
      }
   }

   void add_patch(const Terrain_patch& patch, std::size_t index) noexcept
   {
      std::uint32_t head = patch.info_size;
      const std::uint32_t end = patch.size - 8;

      if (head % 4 != 0) head += (4 - (head % 4));

      while (head < end) {
         const auto& chunk = view_type_as<chunks::Unknown>(patch.bytes[head]);

         if (chunk.mn == "VBUF"_mn) {
            const auto& vbuf = view_type_as<Vbuf>(chunk);

            if (vbuf.element_size == 28) handle_vbuf(vbuf, _patch_offsets[index]);
         }

         head += chunk.size + 8;
         if (head % 4 != 0) head += (4 - (head % 4));
      }
   }

   void save(File_saver& file_saver) noexcept
   {
      const std::size_t file_size = sizeof(Ter_file_header) +
                                    (_heightmap.size() * sizeof(std::uint16_t)) +
                                    ((_colourmap.size() * sizeof(std::uint32_t)) * 2) +
                                    ((_grid_length * _grid_length) / 2) + // water
                                    (_foliage_map.size() * sizeof(std::uint8_t));

      std::string buffer;
      buffer.reserve(file_size);

      buffer.resize(sizeof(Ter_file_header));
      Ter_file_header* header = new (buffer.data()) Ter_file_header{};
      fill_header(*header);

      std::vector<std::int16_t> heightmap;
      std::vector<std::uint32_t> colourmap;

      {
         tbb::task_group tasks;

         tasks.run([this, &heightmap] { heightmap = flip_heightmap(); });
         tasks.run([this, &colourmap] { colourmap = flip_colourmap(); });

         tasks.wait();
      }

      buffer.append(reinterpret_cast<const char*>(heightmap.data()),
                    heightmap.size() * sizeof(std::int16_t));
      buffer.append(reinterpret_cast<const char*>(colourmap.data()),
                    colourmap.size() * sizeof(std::uint32_t));
      buffer.append(reinterpret_cast<const char*>(colourmap.data()),
                    colourmap.size() * sizeof(std::uint32_t));

      file_saver.save_file(std::move(buffer), _name, "world");
   }

private:
   Terrain_builder()
   {
      _texture_scales.fill(0.03125f);
      _texture_rotations.fill(0.0f);
      _texture_axises.fill(0);
   }

   void handle_vbuf(const Vbuf& vbuf, const std::pair<float, float> offset)
   {
      const auto* const vertices =
         reinterpret_cast<const Terrain_vertex*>(&vbuf.bytes[0]);

      for (std::size_t i = 0; i < vbuf.element_count; ++i) {
         handle_vertex(vertices[i], offset);
      }
   };

   void handle_vertex(const Terrain_vertex& vert, const std::pair<float, float> offset)
   {
      const auto x = static_cast<std::size_t>((vert.x + offset.first) / _grid_size);
      const auto z = static_cast<std::size_t>((vert.z + offset.second) / _grid_size);

      if (x == _grid_length || z == _grid_length) return;

      _heightmap[z * _grid_length + x] =
         static_cast<std::int16_t>(vert.y / _height_scale);
      _colourmap[z * _grid_length + x] = vert.colour | 0xFF000000;
   }

   void handle_water_info(const Water_info& info)
   {
      _water_height = info.water_height;
   }

   void handle_water_layer(const Water_layer& layer)
   {
      std::uint32_t head = 0;

      _water_texture = reinterpret_cast<const char*>(&layer.bytes[head]);
      head += static_cast<std::uint32_t>(_water_texture.length()) + 1;

      if (!_water_texture.empty()) _water_texture += ".tga"_sv;

      const auto& layer_info = view_type_as<Water_layer_info>(layer.bytes[head]);

      _water_u_vel = layer_info.u_vel;
      _water_v_vel = layer_info.v_vel * -1.0f;
      _water_u_rept = layer_info.u_rept;
      _water_v_rept = layer_info.v_rept;
      _water_colour = layer_info.colour;
   }

   void implode_foliage(const std::vector<std::uint8_t>& foliage)
   {
      _foliage_map.resize(foliage.size() / 2);

      for (std::size_t i = 0; i < foliage.size(); i += 2) {
         _foliage_map[i / 2] = (foliage[i] << 4) | foliage[i + 1];
      }
   }

   void fill_header(Ter_file_header& header) const
   {
      header.extents[0] = _grid_length / -2;
      header.extents[1] = _grid_length / -2;
      header.extents[2] = _grid_length / 2;
      header.extents[3] = _grid_length / 2;

      std::memcpy(header.tile_range, _texture_scales.data(), sizeof(_texture_scales));
      std::memcpy(header.tile_mapping, _texture_axises.data(), sizeof(_texture_axises));
      std::memcpy(header.tile_rotation, _texture_rotations.data(),
                  sizeof(_texture_rotations));

      header.height_scale = _height_scale;
      header.grid_size = _grid_size;

      header.map_size = _grid_length;

      header.texture_names[0].name[0] = '\x0F';

      for (std::size_t i = 0; i < _textures.size(); ++i) {
         copy_to_cstring(_textures[i], &header.texture_names[i].name[1],
                         sizeof(Ter_file_header::Texture_name::name) - 1);
         copy_to_cstring(_detail_texture, &header.texture_names[i].detail_name[1],
                         sizeof(Ter_file_header::Texture_name::detail_name) - 1);
      }

      header.water[0].height[0] = _water_height;
      header.water[0].height[1] = _water_height;
      header.water[0].u_vel = _water_u_vel;
      header.water[0].v_vel = _water_v_vel;
      header.water[0].u_repeat = _water_u_rept;
      header.water[0].v_repeat = _water_v_rept;
      header.water[0].colour = _water_colour;

      copy_to_cstring(_water_texture, &header.water[0].texture_name[0],
                      sizeof(Ter_file_header::Water::texture_name) - 1);
   }

   std::vector<std::int16_t> flip_heightmap() const
   {
      std::vector<std::int16_t> flipped;
      flipped.resize(_heightmap.size());

      for (std::int32_t y = 0; y < _grid_length; ++y) {
         for (std::int32_t x = 0; x < _grid_length; ++x) {
            flipped[y * _grid_length + x] =
               _heightmap[((_grid_length - 1) - y) * _grid_length + x];
         }
      }

      return flipped;
   }

   std::vector<std::uint32_t> flip_colourmap() const
   {
      std::vector<std::uint32_t> flipped;
      flipped.resize(_colourmap.size());

      for (std::int32_t y = 0; y < _grid_length; ++y) {
         for (std::int32_t x = 0; x < _grid_length; ++x) {
            flipped[y * _grid_length + x] =
               _colourmap[((_grid_length - 1) - y) * _grid_length + x];
         }
      }

      return flipped;
   }

   std::string _name;

   float _grid_size;
   float _height_scale;
   float _height_upscale;

   std::uint16_t _grid_length;

   std::uint16_t _texture_count;

   std::array<std::string, 16> _textures;
   std::string _detail_texture;
   std::array<float, 16> _texture_scales;
   std::array<float, 16> _texture_rotations;
   std::array<std::uint8_t, 16> _texture_axises;

   std::vector<std::pair<float, float>> _patch_offsets;

   std::vector<std::int16_t> _heightmap;
   std::vector<std::uint32_t> _colourmap;
   std::vector<std::uint8_t> _foliage_map;

   float _water_height = 0.0f;
   std::string _water_texture;
   float _water_u_vel;
   float _water_v_vel;
   float _water_u_rept;
   float _water_v_rept;
   std::uint32_t _water_colour;
};

void handle_patches(const Terrain_patches& patches, Terrain_builder& builder)
{
   std::uint32_t head = patches.common_size;
   const std::uint32_t end = patches.size - 8;

   if (head % 4 != 0) head += (4 - (head % 4));

   std::size_t patch_index = 0;

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(patches.bytes[head]);

      if (chunk.mn == "PTCH"_mn) {
         builder.add_patch(view_type_as<Terrain_patch>(patches.bytes[head]), patch_index);

         ++patch_index;
      }

      head += chunk.size + 8;
      if (head % 4 != 0) head += (4 - (head % 4));
   }
}
}

void handle_terrain(const chunks::Terrain& terr, File_saver& file_saver)
{
   std::uint32_t head = 0;
   const std::uint32_t end = terr.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   std::string name{reinterpret_cast<const char*>(&terr.bytes[head]), terr.name_size - 1};

   head += terr.name_size;
   align_head();

   const auto& terrain_info = view_type_as<Terrain_info>(terr.bytes[head]);

   Terrain_builder builder{std::move(name), terrain_info};

   head += sizeof(Terrain_info);

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(terr.bytes[head]);

      if (chunk.mn == "LTEX"_mn) {
         builder.set_textures(view_type_as<Texture_names>(chunk));
      }
      else if (chunk.mn == "DTLX"_mn) {
         builder.set_detail_texture(view_type_as<Dtl_tex_name>(chunk));
      }
      else if (chunk.mn == "SCAL"_mn) {
         builder.set_texture_scales(view_type_as<Texture_metrics<float>>(chunk));
      }
      else if (chunk.mn == "ROTN"_mn) {
         builder.set_texture_rotations(view_type_as<Texture_metrics<float>>(chunk));
      }
      else if (chunk.mn == "AXIS"_mn) {
         builder.set_texture_axises(view_type_as<Texture_metrics<std::uint8_t>>(chunk));
      }
      else if (chunk.mn == "PCHS"_mn) {
         handle_patches(view_type_as<Terrain_patches>(chunk), builder);
      }
      else if (chunk.mn == "FOLG"_mn) {
         builder.set_foliage(view_type_as<Terrain_foliage>(chunk));
      }
      else if (chunk.mn == "WATR"_mn) {
         builder.set_water(view_type_as<Terrain_water>(chunk));
      }

      head += chunk.size + 8;
      align_head();
   }

   builder.save(file_saver);
}