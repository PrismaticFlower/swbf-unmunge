
#include "bit_flags.hpp"
#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"
#include "vbuf_helpers.hpp"

#include "tbb/task_group.h"

#include <array>
#include <tuple>
#include <vector>

namespace {

enum class Material_flags : std::uint32_t {
   normal = 1,
   hardedged = 2,
   singlesided = 4,
   unknown_0 = 8, // Seems to be set when Specular is set.
   glow = 16,
   bumpmap = 32,
   additive = 64,
   specular = 128,
   env_map = 256,
   wireframe = 2048, // Name based off msh flags, may produce some other effect.
   doublesided = 65536,

   scrolling = 16777216,
   energy = 33554432,
   animated = 67108864
};

struct Material_info {
   Material_flags flags;
   std::uint32_t unknown_1;
   std::array<std::uint8_t, 4> colour;
   std::uint32_t specular_intensity;
   std::uint32_t params[2];
};

static_assert(std::is_pod_v<Material_info>);
static_assert(sizeof(Material_info) == 24);

void read_texture_name(Ucfb_reader_strict<"TNAM"_mn> texture_name,
                       std::array<std::string, 4>& out) noexcept
{
   const auto index = texture_name.read_trivial<std::uint32_t>();
   const auto name = texture_name.read_string();

   if (index < out.size()) {
      out[index] = name;
   }
}

auto read_vertex_strip(gsl::span<const std::uint16_t> indices, std::int64_t& pos)
   -> std::vector<std::uint16_t>
{
   if (pos + 2 >= indices.size()) throw std::out_of_range{"Index buffer invalid"};

   std::vector<std::uint16_t> strip;
   strip.reserve(32);

   std::uint16_t last_index = 0xFFFF;

   for (; pos < indices.size(); ++pos) {
      if (indices[pos] == last_index) {
         ++pos;
         break;
      }

      strip.push_back(indices[pos]);
   }

   if (pos + 1 < indices.size()) {
      if (indices[pos] == indices[pos + 1]) ++pos;
   }

   return strip;
}

auto read_index_buffer(Ucfb_reader_strict<"IBUF"_mn> index_buffer)
   -> std::vector<std::vector<std::uint16_t>>
{
   std::vector<std::vector<std::uint16_t>> strips;
   strips.reserve(32);

   const auto indices_count = index_buffer.read_trivial<std::uint32_t>();
   const auto indices = index_buffer.read_array<std::uint16_t>(indices_count);

   for (std::int64_t i = 0; i < indices.size(); ++i) {
      strips.emplace_back(read_vertex_strip(indices, i));
   }

   return strips;
}

std::vector<std::uint8_t> read_bone_map(Ucfb_reader_strict<"BMAP"_mn> bone_map)
{
   const auto count = bone_map.read_trivial<std::uint32_t>();
   const auto bones = bone_map.read_array<std::uint8_t>(count);

   std::vector<std::uint8_t> result;
   result.resize(count);

   std::memcpy(result.data(), bones.data(), result.size());

   return result;
}

std::vector<glm::vec3> read_shadow_vertices(Ucfb_reader_strict<"SHDV"_mn> shadow_vertices)
{
   const auto size = shadow_vertices.size();
   const auto vertex_count = size / sizeof(pod::Vec3);

   std::vector<glm::vec3> vertices_rt{size};

   const auto vertices = shadow_vertices.read_array<pod::Vec3>(vertex_count);

   std::memcpy(vertices_rt.data(), vertices.data(),
               vertices_rt.size() * sizeof(glm::vec3));

   return vertices_rt;
}

auto read_shadow_indices(Ucfb_reader_strict<"SHDI"_mn> shadow_indices)
   -> std::vector<std::vector<std::uint16_t>>
{
   const auto index_count = shadow_indices.size() / sizeof(std::array<std::uint16_t, 4>);

   static_assert(sizeof(std::array<std::uint16_t, 4>) == 8);

   std::vector<std::vector<std::uint16_t>> strips;
   strips.reserve(index_count);

   const auto indices =
      shadow_indices.read_array<std::array<std::uint16_t, 4>>(index_count);

   for (const auto index : indices) {
      strips.emplace_back(
         std::initializer_list<std::uint16_t>{index[0], index[1], index[2]});
   }

   return strips;
}

std::vector<std::uint8_t> read_shadow_skin(Ucfb_reader_strict<"SKIN"_mn> skin)
{
   const auto count = skin.read_trivial<std::uint32_t>();
   skin.consume(4);

   std::vector<std::uint8_t> result;
   result.reserve(count);

   for (std::size_t i = 0; i < count; ++i) {
      skin.consume_unaligned(2);

      result.emplace_back(skin.read_trivial_unaligned<std::uint8_t>());

      skin.consume_unaligned(2);
   }

   return result;
}

void read_material(Ucfb_reader_strict<"MTRL"_mn> material, msh::Material& out)
{
   if (material.size() != sizeof(Material_info)) return;

   const auto info = material.read_trivial<Material_info>();

   out.colour = {info.colour[0] / 255.0f, info.colour[1] / 255.0f,
                 info.colour[2] / 255.0f, info.colour[3] / 255.0f};

   out.specular_value = static_cast<float>(info.specular_intensity);

   out.params[0] = static_cast<std::uint8_t>(info.params[0]);
   out.params[1] = static_cast<std::uint8_t>(info.params[1]);

   if (are_flags_set(info.flags, Material_flags::hardedged)) {
      out.flags = set_flags(out.flags, msh::Render_flags::hardedged);
   }
   if (are_flags_set(info.flags, Material_flags::hardedged)) {
      out.flags = set_flags(out.flags, msh::Render_flags::doublesided);
   }
   if (are_flags_set(info.flags, Material_flags::singlesided)) {
      out.flags = set_flags(out.flags, msh::Render_flags::singlesided);
   }
   if (are_flags_set(info.flags, Material_flags::glow)) {
      out.flags = set_flags(out.flags, msh::Render_flags::glow);
   }
   if (are_flags_set(info.flags, Material_flags::bumpmap)) {
      out.type = msh::Render_type::bumpmap;
   }
   if (are_flags_set(info.flags, Material_flags::additive)) {
      out.flags = set_flags(out.flags, msh::Render_flags::additive);
   }
   if (are_flags_set(info.flags, Material_flags::specular)) {
      out.flags = set_flags(out.flags, msh::Render_flags::specular);
   }
   if (are_flags_set(info.flags, Material_flags::env_map)) {
      out.type = msh::Render_type::env_map;
   }
   if (are_flags_set(info.flags, Material_flags::wireframe)) {
      out.type = msh::Render_type::wireframe;
   }
   if (are_flags_set(info.flags, Material_flags::doublesided)) {
      out.flags = set_flags(out.flags, msh::Render_flags::doublesided);
   }
   if (are_flags_set(info.flags, Material_flags::scrolling)) {
      out.type = msh::Render_type::scrolling;
   }
   if (are_flags_set(info.flags, Material_flags::energy)) {
      out.type = msh::Render_type::energy;
   }
   if (are_flags_set(info.flags, Material_flags::animated)) {
      out.type = msh::Render_type::animated;
   }
}

void read_render_type(Ucfb_reader_strict<"RTYP"_mn> render_type, msh::Material& out)
{
   const auto type = render_type.read_string();

   if (type == "Refraction"_sv) {
      out.type = msh::Render_type::refraction;
   }
}

void read_cshadow(Ucfb_reader_strict<"CSHD"_mn> c_shadow, msh::Shadow& out)
{
   c_shadow.read_child_strict<"INFO"_mn>();
   auto data = c_shadow.read_child_strict<"DATA"_mn>();
   data.read_child_strict<"INFO"_mn>();

   while (data) {
      const auto& child = data.read_child();

      if (child.magic_number() == "SHDV"_mn) {
         out.vertices = read_shadow_vertices(Ucfb_reader_strict<"SHDV"_mn>{child});
      }
      else if (child.magic_number() == "SHDI"_mn) {
         out.strips = read_shadow_indices(Ucfb_reader_strict<"SHDI"_mn>{child});
      }
      else if (child.magic_number() == "SKIN"_mn) {
         out.skin = read_shadow_skin(Ucfb_reader_strict<"SKIN"_mn>{child});
      }
   }
}

void process_segment(Ucfb_reader_strict<"segm"_mn> segment, msh::Builder& builder)
{
   msh::Model model{};

   while (segment) {
      const auto child = segment.read_child();

      if (child.magic_number() == "MTRL"_mn) {
         read_material(Ucfb_reader_strict<"MTRL"_mn>{child}, model.material);
      }
      else if (child.magic_number() == "RTYP"_mn) {
         read_render_type(Ucfb_reader_strict<"RTYP"_mn>{child}, model.material);
      }
      else if (child.magic_number() == "TNAM"_mn) {
         read_texture_name(Ucfb_reader_strict<"TNAM"_mn>{child}, model.material.textures);
      }
      else if (child.magic_number() == "IBUF"_mn) {
         model.strips = read_index_buffer(Ucfb_reader_strict<"IBUF"_mn>{child});
      }
      else if (child.magic_number() == "VBUF"_mn) {
         process_vbuf(Ucfb_reader_strict<"VBUF"_mn>{child}, model);
      }
      else if (child.magic_number() == "BNAM"_mn) {
         model.parent = Ucfb_reader_strict<"BNAM"_mn>{child}.read_string();
      }
      else if (child.magic_number() == "BMAP"_mn) {
         model.bone_map = read_bone_map(Ucfb_reader_strict<"BMAP"_mn>{child});
      }
   }

   builder.add_model(std::move(model));
}

void process_shadow(Ucfb_reader_strict<"shdw"_mn> shadow, msh::Builder& builder)
{
   msh::Shadow msh_shadow{};

   while (shadow) {
      const auto child = shadow.read_child();

      if (child.magic_number() == "CSHD"_mn) {
         read_cshadow(Ucfb_reader_strict<"CSHD"_mn>{child}, msh_shadow);
      }
      else if (child.magic_number() == "BNAM"_mn) {
         msh_shadow.parent = Ucfb_reader_strict<"BNAM"_mn>{child}.read_string();
      }
      else if (child.magic_number() == "BMAP"_mn) {
         msh_shadow.bone_map = read_bone_map(Ucfb_reader_strict<"BMAP"_mn>{child});
      }
   }

   builder.add_shadow(std::move(msh_shadow));
}
}

void handle_model(Ucfb_reader model, msh::Builders_map& builders, tbb::task_group& tasks)
{
   const std::string name{model.read_child_strict<"NAME"_mn>().read_string()};

   auto& builder = builders[name];

   while (model) {
      const auto child = model.read_child();

      if (child.magic_number() == "segm"_mn) {
         tasks.run([child, &builder] {
            process_segment(Ucfb_reader_strict<"segm"_mn>{child}, builder);
         });
      }
      else if (child.magic_number() == "shdw"_mn) {
         tasks.run([child, &builder] {
            process_shadow(Ucfb_reader_strict<"shdw"_mn>{child}, builder);
         });
      }
   }
}
