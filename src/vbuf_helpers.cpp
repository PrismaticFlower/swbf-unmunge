
#include "vbuf_helpers.hpp"
#include "magic_number.hpp"
#include "type_pun.hpp"

#define GLM_FORCE_CXX98
#include "glm/glm.hpp"

#include <array>

namespace {

static_assert(std::is_standard_layout_v<glm::vec2>);
static_assert(sizeof(glm::vec2) == 8);
static_assert(std::is_standard_layout_v<glm::vec3>);
static_assert(sizeof(glm::vec3) == 12);
static_assert(std::is_standard_layout_v<glm::vec4>);
static_assert(sizeof(glm::vec4) == 16);
static_assert(std::is_standard_layout_v<std::array<std::uint8_t, 4>>);
static_assert(sizeof(std::array<std::uint8_t, 4>) == 4);

#pragma pack(push, 1)
#pragma warning(disable : 4200)

template<typename Entry>
struct Vbuf_template {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t entry_count;
   std::uint32_t entry_size;
   Vbuf_types type;

   Entry entries[];
};

struct Vbuf_xyznuv_entry {
   glm::vec3 position;
   glm::vec3 normal;
   glm::vec2 uv;
};

static_assert(std::is_standard_layout_v<Vbuf_xyznuv_entry>);
static_assert(sizeof(Vbuf_xyznuv_entry) == 32);

using Vbuf_xyznuv = Vbuf_template<Vbuf_xyznuv_entry>;

static_assert(std::is_standard_layout_v<Vbuf_xyznuv>);
static_assert(sizeof(Vbuf_xyznuv) == 20);

struct Vbuf_xyzncuv_entry {
   glm::vec3 position;
   glm::vec3 normal;
   std::array<std::uint8_t, 4> rgba;
   glm::vec2 uv;
};

static_assert(std::is_standard_layout_v<Vbuf_xyzncuv_entry>);
static_assert(sizeof(Vbuf_xyzncuv_entry) == 36);

using Vbuf_xyzncuv = Vbuf_template<Vbuf_xyzncuv_entry>;

static_assert(std::is_standard_layout_v<Vbuf_xyzncuv>);
static_assert(sizeof(Vbuf_xyzncuv) == 20);

struct Vbuf_xyzsknuv_entry {
   glm::vec3 position;
   std::array<std::uint8_t, 4> skin;
   glm::vec3 normal;
   glm::vec2 uv;
};

static_assert(std::is_standard_layout_v<Vbuf_xyzsknuv_entry>);
static_assert(sizeof(Vbuf_xyzsknuv_entry) == 36);

using Vbuf_xyzsknuv = Vbuf_template<Vbuf_xyzsknuv_entry>;

static_assert(std::is_standard_layout_v<Vbuf_xyzsknuv>);
static_assert(sizeof(Vbuf_xyzsknuv) == 20);

#pragma pack(pop)

glm::vec2 flip_texture_y(glm::vec2 tex_coord)
{
   return {tex_coord.x, 1.0f - tex_coord.y};
}

void process_vbuf_impl(const Vbuf_xyznuv& vbuf, msh::Model& model)
{
   model.vertices.clear();
   model.vertices.reserve(vbuf.entry_count);
   model.normals.clear();
   model.normals.reserve(vbuf.entry_count);
   model.texture_coords.clear();
   model.texture_coords.reserve(vbuf.entry_count);

   for (std::size_t i = 0; i < vbuf.entry_count; i++) {
      const auto& entry = vbuf.entries[i];

      model.vertices.push_back(entry.position);
      model.normals.push_back(entry.normal);
      model.texture_coords.push_back(flip_texture_y(entry.uv));
   }
}

void process_vbuf_impl(const Vbuf_xyzncuv& vbuf, msh::Model& model)
{
   model.vertices.clear();
   model.vertices.reserve(vbuf.entry_count);
   model.normals.clear();
   model.normals.reserve(vbuf.entry_count);
   model.colours.clear();
   model.colours.reserve(vbuf.entry_count);
   model.texture_coords.clear();
   model.texture_coords.reserve(vbuf.entry_count);

   for (std::size_t i = 0; i < vbuf.entry_count; i++) {
      const auto& entry = vbuf.entries[i];

      model.vertices.push_back(entry.position);
      model.normals.push_back(entry.normal);
      model.colours.push_back(entry.rgba);
      model.texture_coords.push_back(flip_texture_y(entry.uv));
   }
}

void process_vbuf_impl(const Vbuf_xyzsknuv& vbuf, msh::Model& model)
{
   model.vertices.clear();
   model.vertices.reserve(vbuf.entry_count);
   model.skin.clear();
   model.skin.reserve(vbuf.entry_count);
   model.normals.clear();
   model.normals.reserve(vbuf.entry_count);
   model.texture_coords.clear();
   model.texture_coords.reserve(vbuf.entry_count);

   for (std::size_t i = 0; i < vbuf.entry_count; i++) {
      const auto& entry = vbuf.entries[i];

      model.vertices.push_back(entry.position);
      model.skin.push_back(entry.skin[0]);
      model.normals.push_back(entry.normal);
      model.texture_coords.push_back(flip_texture_y(entry.uv));
   }
}
}

void process_vbuf(const Vbuf& vbuf, msh::Model& model)
{
   if (vbuf.type == Vbuf_types::xyznuv) {
      process_vbuf_impl(view_type_as<Vbuf_xyznuv>(vbuf), model);
   }
   else if (vbuf.type == Vbuf_types::xyzncuv || vbuf.type == Vbuf_types::xyzncuv_2) {
      process_vbuf_impl(view_type_as<Vbuf_xyzncuv>(vbuf), model);
   }
   else if (vbuf.type == Vbuf_types::xyzsknuv) {
      process_vbuf_impl(view_type_as<Vbuf_xyzsknuv>(vbuf), model);
   }
}