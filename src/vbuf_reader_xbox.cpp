
#include "msh_builder.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include <array>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

using namespace std::string_literals;

namespace {

enum class Vbuf_type : std::uint32_t {
   textured = 0x0000d222,                               // 14 - stride
   textured_vertex_lit = 0x0000d322,                    // 18 - stride
   textured_vertex_coloured = 0x0000d2a2,               // 18 - stride
   textured_hardskinned = 0x0000f226,                   // 15 - stride
   textured_softskinned = 0x0000f22e,                   // 19 - stride
   textured_normal_mapped = 0x0000d262,                 // 22 - stride
   textured_normal_mapped_vertex_coloured = 0x0000d2e2, // 26 - stride
   textured_normal_mapped_vertex_lit = 0x0000d362,      // 26 - stride
   textured_hardskinned_normal_mapped = 0x0000f266,     // 23 - stride
   textured_softskinned_normal_mapped = 0x0000f26e,     // 27 - stride
};

struct Vbuf_info {
   std::uint32_t count;
   std::uint32_t stride;
   Vbuf_type type;
};

static_assert(std::is_pod_v<Vbuf_info>);
static_assert(sizeof(Vbuf_info) == 12);

glm::vec3 decompress_position(const std::array<std::int16_t, 3> ushort_position,
                              const std::array<glm::vec3, 2> vert_box)
{
   glm::vec3 position{glm::ctor::uninitialize};

   position.x = glm::mix(vert_box[0].x, vert_box[1].x,
                         (static_cast<float>(ushort_position[0]) / 32767.f + 1.f) * 0.5f);
   position.y = glm::mix(vert_box[0].y, vert_box[1].y,
                         (static_cast<float>(ushort_position[1]) / 32767.f + 1.f) * 0.5f);
   position.z = glm::mix(vert_box[0].z, vert_box[1].z,
                         (static_cast<float>(ushort_position[2]) / 32767.f + 1.f) * 0.5f);

   return position;
}

glm::vec3 decompress_weights(const std::array<std::uint8_t, 2> ubyte_weights)
{
   constexpr auto factor = 1.f / 255.f;

   const auto x = static_cast<float>(ubyte_weights[0]) / 255.f;
   const auto y = static_cast<float>(ubyte_weights[1]) / 255.f;
   const auto z = 1.f - x - y;

   return glm::vec3{x, y, z};
}

glm::vec3 decompress_normal(const std::uint32_t udec_normal)
{
   constexpr std::array<std::uint32_t, 2> sign_extend_xy = {0x0u, 0xfffffc00u};
   constexpr std::array<std::uint32_t, 2> sign_extend_z = {0x0u, 0xfffff800u};

   const auto x_unsigned = udec_normal & 0x7ffu;
   const auto y_unsigned = (udec_normal >> 11u) & 0x7ffu;
   const auto z_unsigned = (udec_normal >> 22u) & 0x3ffu;

   const auto x_signed =
      static_cast<std::int32_t>(x_unsigned | sign_extend_xy[x_unsigned >> 10u]);
   const auto y_signed =
      static_cast<std::int32_t>(y_unsigned | sign_extend_xy[y_unsigned >> 10u]);
   const auto z_signed =
      static_cast<std::int32_t>(z_unsigned | sign_extend_z[z_unsigned >> 9u]);

   const auto x = static_cast<float>(x_signed) / 1023.f;
   const auto y = static_cast<float>(y_signed) / 1023.f;
   const auto z = static_cast<float>(z_signed) / 511.f;

   return glm::vec3{x, y, z};
}

glm::vec2 decompress_texcoords(const std::array<std::uint16_t, 2> ushort_texcoords)
{
   constexpr auto factor = 1.f / 2048.f;

   return glm::vec2{static_cast<float>(ushort_texcoords[0]) * factor,
                    static_cast<float>(ushort_texcoords[1]) * factor};
}

glm::vec3 get_position(Ucfb_reader_strict<"VBUF"_mn>& vbuf,
                       const std::array<glm::vec3, 2> vert_box)
{
   const auto ushort_position =
      vbuf.read_trivial_unaligned<std::array<std::int16_t, 3>>();

   return decompress_position(ushort_position, vert_box);
}

glm::u8vec3 get_bone_index(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto bone = vbuf.read_trivial_unaligned<std::uint8_t>();

   return glm::u8vec3{bone};
}

glm::u8vec3 get_bone_indices(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto bones = vbuf.read_trivial_unaligned<std::array<std::uint8_t, 3>>();

   return glm::u8vec3{bones[0], bones[1], bones[2]};
}

glm::vec3 get_bone_weights(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto ubyte_weights = vbuf.read_trivial_unaligned<std::array<std::uint8_t, 2>>();

   return decompress_weights(ubyte_weights);
}

glm::vec3 get_normal(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto udec_normal = vbuf.read_trivial_unaligned<std::uint32_t>();

   return decompress_normal(udec_normal);
}

glm::vec4 get_colour(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto uint_colour = vbuf.read_trivial_unaligned<std::uint32_t>();

   return glm::unpackUnorm4x8(uint_colour);
}

glm::vec2 get_texcoords(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto ushort_texcoords =
      vbuf.read_trivial_unaligned<std::array<std::uint16_t, 2>>();

   return decompress_texcoords(ushort_texcoords);
}

template<std::uint32_t expected>
void expect_stride(const Vbuf_info& info)
{
   if (info.stride != expected) {
      const auto message = "Unexpected stride value in VBUF:"s + "\n   VBUF type: "s +
                           to_hexstring(static_cast<std::uint32_t>(info.type)) +
                           "\n   vertex count: "s + std::to_string(info.count) +
                           "\n   stride: "s + std::to_string(info.stride) +
                           "\n   expected stride: "s + std::to_string(expected);

      throw std::runtime_error{message};
   }
}

void read_textured(Ucfb_reader_strict<"VBUF"_mn> vbuf, Vbuf_info info, msh::Model& model,
                   const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<14>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);
      model.normals[i] = get_normal(vbuf);
      model.texture_coords[i] = get_texcoords(vbuf);
   }
}

void read_textured_coloured(Ucfb_reader_strict<"VBUF"_mn> vbuf, Vbuf_info info,
                            msh::Model& model, const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<18>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.colours.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);
      model.normals[i] = get_normal(vbuf);
      model.colours[i] = get_colour(vbuf);
      model.texture_coords[i] = get_texcoords(vbuf);
   }
}

void read_textured_hardskinned(Ucfb_reader_strict<"VBUF"_mn> vbuf, Vbuf_info info,
                               msh::Model& model, const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<15>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);

      model.skin[i].bones = get_bone_index(vbuf);
      model.skin[i].weights = glm::vec3{1.f, 0.0f, 0.0f};

      model.normals[i] = get_normal(vbuf);
      model.texture_coords[i] = get_texcoords(vbuf);
   }
}

void read_textured_softskinned(Ucfb_reader_strict<"VBUF"_mn> vbuf, Vbuf_info info,
                               msh::Model& model, const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<19>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);

      model.skin[i].weights = get_bone_weights(vbuf);
      model.skin[i].bones = get_bone_indices(vbuf);

      model.normals[i] = get_normal(vbuf);
      model.texture_coords[i] = get_texcoords(vbuf);
   }
}

void read_textured_normal_mapped(Ucfb_reader_strict<"VBUF"_mn> vbuf, Vbuf_info info,
                                 msh::Model& model,
                                 const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<22>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);
      model.normals[i] = get_normal(vbuf);

      get_normal(vbuf); // tangent
      get_normal(vbuf); // bitangent

      model.texture_coords[i] = get_texcoords(vbuf);
   }
}

void read_textured_normal_mapped_coloured(Ucfb_reader_strict<"VBUF"_mn> vbuf,
                                          Vbuf_info info, msh::Model& model,
                                          const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<26>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.colours.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);
      model.normals[i] = get_normal(vbuf);

      get_normal(vbuf); // tangent
      get_normal(vbuf); // bitangent

      model.colours[i] = get_colour(vbuf);
      model.texture_coords[i] = get_texcoords(vbuf);
   }
}

void read_textured_hardskinned_normal_mapped(Ucfb_reader_strict<"VBUF"_mn> vbuf,
                                             Vbuf_info info, msh::Model& model,
                                             const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<23>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);

      model.skin[i].bones = get_bone_index(vbuf);
      model.skin[i].weights = glm::vec3{1.f, 0.0f, 0.0f};

      model.normals[i] = get_normal(vbuf);

      get_normal(vbuf); // tangent
      get_normal(vbuf); // bitangent

      model.texture_coords[i] = get_texcoords(vbuf);
   }
}

void read_textured_softskinned_normal_mapped(Ucfb_reader_strict<"VBUF"_mn> vbuf,
                                             Vbuf_info info, msh::Model& model,
                                             const std::array<glm::vec3, 2> vert_box)
{
   expect_stride<27>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = get_position(vbuf, vert_box);

      model.skin[i].weights = get_bone_weights(vbuf);
      model.skin[i].bones = get_bone_indices(vbuf);

      model.normals[i] = get_normal(vbuf);

      get_normal(vbuf); // tangent
      get_normal(vbuf); // bitangent

      model.texture_coords[i] = get_texcoords(vbuf);
   }
}
}

void read_vbuf_xbox(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model,
                    const std::array<glm::vec3, 2> vert_box, bool* const pretransformed)
{
   const auto info = vbuf.read_trivial<Vbuf_info>();

   switch (info.type) {
   case Vbuf_type::textured:
      return read_textured(vbuf, info, model, vert_box);
   case Vbuf_type::textured_vertex_lit:
   case Vbuf_type::textured_vertex_coloured:
      return read_textured_coloured(vbuf, info, model, vert_box);
   case Vbuf_type::textured_hardskinned:
      if (pretransformed) *pretransformed = true;

      return read_textured_hardskinned(vbuf, info, model, vert_box);
   case Vbuf_type::textured_softskinned:
      return read_textured_softskinned(vbuf, info, model, vert_box);
   case Vbuf_type::textured_normal_mapped:
      return read_textured_normal_mapped(vbuf, info, model, vert_box);
   case Vbuf_type::textured_normal_mapped_vertex_lit:
   case Vbuf_type::textured_normal_mapped_vertex_coloured:
      return read_textured_normal_mapped_coloured(vbuf, info, model, vert_box);
   case Vbuf_type::textured_hardskinned_normal_mapped:
      if (pretransformed) *pretransformed = true;

      return read_textured_hardskinned_normal_mapped(vbuf, info, model, vert_box);
   case Vbuf_type::textured_softskinned_normal_mapped:
      return read_textured_softskinned_normal_mapped(vbuf, info, model, vert_box);
   default:
      synced_cout::print("Warning: Unknown Xbox VBUF encountered."s, "\n   size : "s,
                         vbuf.size(), "\n   entry count: "s, info.count, "\n   stride: "s,
                         info.stride, "\n   entry type: "s,
                         to_hexstring(static_cast<std::uint32_t>(info.type)), '\n');
   }
}
