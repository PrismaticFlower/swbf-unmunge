
#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>

using namespace std::literals;

namespace {

enum class Vbuf_type : std::uint32_t {
   textured = 0x00000222,                           // stride - 32
   textured_coloured = 0x000002a2,                  // stride - 36
   textured_vertex_lit = 0x00000322,                // stride - 36
   textured_hardskinned = 0x00000226,               // stride - 36
   textured_softskinned = 0x0000022e,               // stride - 44
   textured_normal_mapped = 0x00000262,             // stride - 56
   textured_normal_mapped_coloured = 0x000002e2,    // stride - 60
   textured_normal_mapped_vertex_lit = 0x00000362,  // stride - 60
   textured_normal_mapped_hardskinned = 0x00000266, // stride - 60
   textured_normal_mapped_softskinned = 0x0000026e, // stride - 68
};

struct Vbuf_info {
   std::uint32_t count;
   std::uint32_t stride;
   Vbuf_type type;
};

static_assert(std::is_pod_v<Vbuf_info>);
static_assert(sizeof(Vbuf_info) == 12);

void print_failed_search(const std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs)
{
   std::stringstream stream;

   for (auto vbuf : vbufs) {
      const auto info = vbuf.read_trivial<Vbuf_info>();

      stream << "\n   VBUF:";
      stream << "\n      size:" << vbuf.size();
      stream << "\n      count:" << info.count;
      stream << "\n      stride:" << info.stride;
      stream << "\n      type:" << to_hexstring(static_cast<std::uint32_t>(info.type));
   }

   synced_cout::print("Warning: Unable to find usable VBUF type options are:"s,
                      stream.str(), '\n');
}

auto find_best_usable_vbuf(const std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs)
   -> std::optional<Ucfb_reader_strict<"VBUF"_mn>>
{
   const auto begin = std::cbegin(vbufs);
   const auto end = std::cend(vbufs);

   const auto find_type = [&](Vbuf_type type) {
      const auto checker = [type](Ucfb_reader_strict<"VBUF"_mn> vbuf) {
         const auto info = vbuf.read_trivial<Vbuf_info>();

         return (info.type == type);
      };

      return std::find_if(begin, end, checker);
   };

   if (auto result = find_type(Vbuf_type::textured_softskinned); result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_normal_mapped_softskinned);
            result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_hardskinned); result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_normal_mapped_hardskinned);
            result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured); result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_coloured); result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_vertex_lit); result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_normal_mapped_coloured);
            result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_normal_mapped_vertex_lit);
            result != end) {
      return *result;
   }
   else if (result = find_type(Vbuf_type::textured_normal_mapped); result != end) {
      return *result;
   }

   return std::nullopt;
}

bool is_pretransformed_vbuf(const Vbuf_type type) noexcept
{
   switch (type) {
   case Vbuf_type::textured_hardskinned:
   case Vbuf_type::textured_normal_mapped_hardskinned:
      return true;
   default:
      return false;
   }
}

glm::vec4 read_colour(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto colour = vbuf.read_trivial<std::uint32_t>();

   return glm::unpackUnorm4x8(colour);
}

glm::vec3 read_weights(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto weights = vbuf.read_trivial<pod::Vec2>();

   return glm::vec3{weights.x, weights.y, 1.f - weights.x - weights.y};
}

glm::u8vec3 read_index(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto indices = vbuf.read_trivial<std::uint32_t>();

   return glm::u8vec3{static_cast<std::uint8_t>(indices & 0xffu)};
}

glm::u8vec3 read_indices(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto packed = vbuf.read_trivial<std::uint32_t>();

   glm::u8vec3 indices;

   indices.x = static_cast<std::uint8_t>(packed & 0xffu);
   indices.y = static_cast<std::uint8_t>((packed >> 8u) & 0xffu);
   indices.z = static_cast<std::uint8_t>((packed >> 16u) & 0xffu);

   return indices;
}

glm::vec2 read_texcoords(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto coords = vbuf.read_trivial<pod::Vec2>();

   return {coords.x, 1.f - glm::fract(coords.y)};
}

template<std::uint32_t expected>
void expect_stride(const Vbuf_info& info)
{
   if (info.stride != expected) {
      const auto message = "Unexpected stride value in VBUF:"s + "\n   type: "s +
                           to_hexstring(static_cast<std::uint32_t>(info.type)) +
                           "\n   vertex count: "s + std::to_string(info.count) +
                           "\n   stride: "s + std::to_string(info.stride) +
                           "\n   expected stride: "s + std::to_string(expected);

      throw std::runtime_error{message};
   }
}

void read_textured(Ucfb_reader_strict<"VBUF"_mn> vbuf, const Vbuf_info info,
                   msh::Model& model)
{
   expect_stride<32u>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();
      model.normals[i] = vbuf.read_trivial<pod::Vec3>();
      model.texture_coords[i] = read_texcoords(vbuf);
   }
}

void read_textured_coloured(Ucfb_reader_strict<"VBUF"_mn> vbuf, const Vbuf_info info,
                            msh::Model& model)
{
   expect_stride<36u>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.colours.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();
      model.normals[i] = vbuf.read_trivial<pod::Vec3>();
      model.colours[i] = read_colour(vbuf);
      model.texture_coords[i] = read_texcoords(vbuf);
   }
}

void read_textured_hardskinned(Ucfb_reader_strict<"VBUF"_mn> vbuf, const Vbuf_info info,
                               msh::Model& model)
{
   expect_stride<36u>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();
      model.skin[i].weights = glm::vec3{1.f, 0.f, 0.f};
      model.skin[i].bones = read_index(vbuf);
      model.normals[i] = vbuf.read_trivial<pod::Vec3>();
      model.texture_coords[i] = read_texcoords(vbuf);
   }
}

void read_textured_softskinned(Ucfb_reader_strict<"VBUF"_mn> vbuf, const Vbuf_info info,
                               msh::Model& model)
{
   expect_stride<44u>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();
      model.skin[i].weights = read_weights(vbuf);
      model.skin[i].bones = read_indices(vbuf);
      model.normals[i] = vbuf.read_trivial<pod::Vec3>();
      model.texture_coords[i] = read_texcoords(vbuf);
   }
}

void read_textured_normal_mapped(Ucfb_reader_strict<"VBUF"_mn> vbuf, const Vbuf_info info,
                                 msh::Model& model)
{
   expect_stride<56u>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();
      model.normals[i] = vbuf.read_trivial<pod::Vec3>();

      vbuf.read_trivial<pod::Vec3>(); // tangent
      vbuf.read_trivial<pod::Vec3>(); // bitangent

      model.texture_coords[i] = read_texcoords(vbuf);
   }
}

void read_textured_coloured_normal_mapped(Ucfb_reader_strict<"VBUF"_mn> vbuf,
                                          const Vbuf_info info, msh::Model& model)
{
   expect_stride<60u>(info);

   model.positions.resize(info.count);
   model.normals.resize(info.count);
   model.colours.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();

      model.normals[i] = vbuf.read_trivial<pod::Vec3>();
      vbuf.read_trivial<pod::Vec3>(); // tangent
      vbuf.read_trivial<pod::Vec3>(); // bitangent

      model.colours[i] = read_colour(vbuf);
      model.texture_coords[i] = read_texcoords(vbuf);
   }
}

void read_textured_normal_mapped_hardskinned(Ucfb_reader_strict<"VBUF"_mn> vbuf,
                                             const Vbuf_info info, msh::Model& model)
{
   expect_stride<60u>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();
      model.skin[i].weights = glm::vec3{1.f, 0.f, 0.f};
      model.skin[i].bones = read_index(vbuf);

      model.normals[i] = vbuf.read_trivial<pod::Vec3>();
      vbuf.read_trivial<pod::Vec3>(); // tangent
      vbuf.read_trivial<pod::Vec3>(); // bitangent

      model.texture_coords[i] = read_texcoords(vbuf);
   }
}

void read_textured_normal_mapped_softskinned(Ucfb_reader_strict<"VBUF"_mn> vbuf,
                                             const Vbuf_info info, msh::Model& model)
{
   expect_stride<68u>(info);

   model.positions.resize(info.count);
   model.skin.resize(info.count);
   model.normals.resize(info.count);
   model.texture_coords.resize(info.count);

   for (auto i = 0u; i < info.count; ++i) {
      model.positions[i] = vbuf.read_trivial<pod::Vec3>();
      model.skin[i].weights = read_weights(vbuf);
      model.skin[i].bones = read_indices(vbuf);

      model.normals[i] = vbuf.read_trivial<pod::Vec3>();
      vbuf.read_trivial<pod::Vec3>(); // tangent
      vbuf.read_trivial<pod::Vec3>(); // bitangent

      model.texture_coords[i] = read_texcoords(vbuf);
   }
}
}

void read_vbuf(const std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs, msh::Model& model,
               bool* const pretransformed)
{
   auto vbuf = find_best_usable_vbuf(vbufs);

   if (!vbuf) {
      print_failed_search(vbufs);

      return;
   }

   const auto info = vbuf->read_trivial<Vbuf_info>();

   if (pretransformed) *pretransformed = is_pretransformed_vbuf(info.type);

   switch (info.type) {
   case Vbuf_type::textured:
      return read_textured(*vbuf, info, model);
   case Vbuf_type::textured_coloured:
   case Vbuf_type::textured_vertex_lit:
      return read_textured_coloured(*vbuf, info, model);
   case Vbuf_type::textured_hardskinned:
      return read_textured_hardskinned(*vbuf, info, model);
   case Vbuf_type::textured_softskinned:
      return read_textured_softskinned(*vbuf, info, model);
   case Vbuf_type::textured_normal_mapped:
      return read_textured_normal_mapped(*vbuf, info, model);
   case Vbuf_type::textured_normal_mapped_coloured:
   case Vbuf_type::textured_normal_mapped_vertex_lit:
      return read_textured_coloured_normal_mapped(*vbuf, info, model);
   case Vbuf_type::textured_normal_mapped_hardskinned:
      return read_textured_normal_mapped_hardskinned(*vbuf, info, model);
   case Vbuf_type::textured_normal_mapped_softskinned:
      return read_textured_normal_mapped_softskinned(*vbuf, info, model);
   }
}
