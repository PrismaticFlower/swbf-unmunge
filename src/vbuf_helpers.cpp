
#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include <array>
#include <cstdint>

namespace {

enum class Vbuf_types : std::uint32_t {
   // xyz (position) - xyz (normal) - uv (texture coordinates)
   xyznuv = 0x00000222,
   // xyz (position) - xyz (normal) - rgba - uv (texture coordinates)
   xyzncuv = 0x0000322,
   // xyz (position) - xyz (normal) - rgba - uv (texture coordinates)
   xyzncuv_2 = 0x000002a2,

   // xyz (position) - skin - xyz (normal) - uv (texture coordinates)
   xyzsknuv = 0x0000226,

   unknown_16 = 0x0000d222,
   unknown_20 = 0x0000f226
};

struct Vbuf_info {
   std::uint32_t entry_count;
   std::uint32_t entry_size;
   Vbuf_types type;
};

static_assert(std::is_pod_v<Vbuf_info>);
static_assert(sizeof(Vbuf_info) == 12);

struct Vbuf_xyznuv_entry {
   pod::Vec3 position;
   pod::Vec3 normal;
   pod::Vec2 uv;
};

static_assert(std::is_pod_v<Vbuf_xyznuv_entry>);
static_assert(sizeof(Vbuf_xyznuv_entry) == 32);

struct Vbuf_xyzncuv_entry {
   pod::Vec3 position;
   pod::Vec3 normal;
   std::array<std::uint8_t, 4> rgba;
   pod::Vec2 uv;
};

static_assert(std::is_pod_v<Vbuf_xyzncuv_entry>);
static_assert(sizeof(Vbuf_xyzncuv_entry) == 36);

struct Vbuf_xyzsknuv_entry {
   pod::Vec3 position;
   std::array<std::uint8_t, 4> skin;
   pod::Vec3 normal;
   pod::Vec2 uv;
};

static_assert(std::is_pod_v<Vbuf_xyzsknuv_entry>);
static_assert(sizeof(Vbuf_xyzsknuv_entry) == 36);

glm::vec2 flip_texture_y(glm::vec2 tex_coord)
{
   return {tex_coord.x, 1.0f - tex_coord.y};
}

void process_vbuf_impl(gsl::span<const Vbuf_xyznuv_entry> entries, msh::Model& model)
{
   model.vertices.clear();
   model.vertices.reserve(entries.size());
   model.normals.clear();
   model.normals.reserve(entries.size());
   model.texture_coords.clear();
   model.texture_coords.reserve(entries.size());

   for (const auto& entry : entries) {
      model.vertices.push_back(entry.position);
      model.normals.push_back(entry.normal);
      model.texture_coords.push_back(flip_texture_y(entry.uv));
   }
}

void process_vbuf_impl(gsl::span<const Vbuf_xyzncuv_entry> entries, msh::Model& model)
{
   model.vertices.clear();
   model.vertices.reserve(entries.size());
   model.normals.clear();
   model.normals.reserve(entries.size());
   model.colours.clear();
   model.colours.reserve(entries.size());
   model.texture_coords.clear();
   model.texture_coords.reserve(entries.size());

   for (const auto& entry : entries) {
      model.vertices.push_back(entry.position);
      model.normals.push_back(entry.normal);
      model.colours.push_back(entry.rgba);
      model.texture_coords.push_back(flip_texture_y(entry.uv));
   }
}

void process_vbuf_impl(gsl::span<const Vbuf_xyzsknuv_entry> entries, msh::Model& model)
{
   model.vertices.clear();
   model.vertices.reserve(entries.size());
   model.skin.clear();
   model.skin.reserve(entries.size());
   model.normals.clear();
   model.normals.reserve(entries.size());
   model.texture_coords.clear();
   model.texture_coords.reserve(entries.size());

   for (const auto& entry : entries) {
      model.vertices.push_back(entry.position);
      model.skin.push_back(entry.skin[0]);
      model.normals.push_back(entry.normal);
      model.texture_coords.push_back(flip_texture_y(entry.uv));
   }
}
}

void process_vbuf(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model)
{
   const auto vbuf_info = vbuf.read_trivial<Vbuf_info>();

   if (vbuf_info.type == Vbuf_types::xyznuv) {
      process_vbuf_impl(vbuf.read_array<Vbuf_xyznuv_entry>(vbuf_info.entry_count), model);
   }
   else if (vbuf_info.type == Vbuf_types::xyzncuv ||
            vbuf_info.type == Vbuf_types::xyzncuv_2) {
      process_vbuf_impl(vbuf.read_array<Vbuf_xyzncuv_entry>(vbuf_info.entry_count),
                        model);
   }
   else if (vbuf_info.type == Vbuf_types::xyzsknuv) {
      process_vbuf_impl(vbuf.read_array<Vbuf_xyzsknuv_entry>(vbuf_info.entry_count),
                        model);
   }
}