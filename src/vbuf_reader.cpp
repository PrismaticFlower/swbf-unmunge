
#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include <array>
#include <cmath>
#include <cstdint>

using namespace std::literals;

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

   // "compressed" formats
   unused_12 = 0x00005022,
   unused_16 = 0x0000d222,
   unused_20_a = 0x0000d2a2,
   unused_20_b = 0x0000f226,
   unused_20_c = 0x0000d322,
   unused_24 = 0x0000d262,
   unused_28_a = 0x0000780e,
   unused_28_b = 0x0000d2e2,

   // uncompressed formats for which an alternate, shorter representation can be
   // used to reverse the mesh instead
   unused_56 = 0x00000262,
   unused_60 = 0x000002e2,
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

constexpr bool is_known_vbuf(Vbuf_types type) noexcept
{
   switch (type) {
   case Vbuf_types::xyznuv:
      return true;
   case Vbuf_types::xyzncuv:
      return true;
   case Vbuf_types::xyzncuv_2:
      return true;
   case Vbuf_types::xyzsknuv:
      return true;
   case Vbuf_types::unused_12:
      return true;
   case Vbuf_types::unused_16:
      return true;
   case Vbuf_types::unused_20_a:
      return true;
   case Vbuf_types::unused_20_b:
      return true;
   case Vbuf_types::unused_20_c:
      return true;
   case Vbuf_types::unused_24:
      return true;
   case Vbuf_types::unused_28_a:
      return true;
   case Vbuf_types::unused_28_b:
      return true;
   case Vbuf_types::unused_56:
      return true;
   case Vbuf_types::unused_60:
      return true;
   default:
      return false;
   }
}

glm::vec2 flip_texture_v(const glm::vec2 coords) noexcept
{
   float v = coords.y;

   if (v > 1.0f) {
      v = std::fmod(v, 1.0f);
   }

   return {coords.x, 1.0f - v};
}

void read_vbuf_span(gsl::span<const Vbuf_xyznuv_entry> entries, msh::Model& model)
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
      model.texture_coords.push_back(flip_texture_v(entry.uv));
   }
}

void read_vbuf_span(gsl::span<const Vbuf_xyzncuv_entry> entries, msh::Model& model)
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
      model.texture_coords.push_back(entry.uv);
   }
}

void read_vbuf_span(gsl::span<const Vbuf_xyzsknuv_entry> entries, msh::Model& model)
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
      model.texture_coords.push_back(flip_texture_v(entry.uv));
   }
}
}

void read_vbuf(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model)
{
   const auto vbuf_info = vbuf.read_trivial<Vbuf_info>();

   if (vbuf_info.type == Vbuf_types::xyznuv) {
      read_vbuf_span(vbuf.read_array<Vbuf_xyznuv_entry>(vbuf_info.entry_count), model);
   }
   else if (vbuf_info.type == Vbuf_types::xyzncuv ||
            vbuf_info.type == Vbuf_types::xyzncuv_2) {
      read_vbuf_span(vbuf.read_array<Vbuf_xyzncuv_entry>(vbuf_info.entry_count), model);
   }
   else if (vbuf_info.type == Vbuf_types::xyzsknuv) {
      read_vbuf_span(vbuf.read_array<Vbuf_xyzsknuv_entry>(vbuf_info.entry_count), model);
   }
   else if (!is_known_vbuf(vbuf_info.type)) {
      synced_cout::print("Warning: Unknown VBUF encountered."s, "\n   size : "s,
                         vbuf.size(), "\n   entry count: "s, vbuf_info.entry_count,
                         "\n   entry size: "s, vbuf_info.entry_size, "\n   entry type: "s,
                         to_hexstring(static_cast<std::uint32_t>(vbuf_info.type)), '\n');
   }
}
