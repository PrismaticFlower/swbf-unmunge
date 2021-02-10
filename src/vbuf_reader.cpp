
#include "vbuf_reader.hpp"
#include "bit_flags.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <sstream>

using namespace std::literals;

namespace {

enum class Vbuf_flags : std::uint32_t {
   none = 0b0u,
   position = 0b10u,
   bone_indices = 0b100u,
   bone_weights = 0b1000u,
   normal = 0b100000u,
   tangents = 0b1000000u,
   color = 0b10000000u,
   static_lighting = 0b100000000u,
   texcoords = 0b1000000000u,

   shadow_data = 0b100000000000u,

   position_compressed = 0b1000000000000u,
   bone_info_compressed = 0b10000000000000u,
   normal_compressed = 0b100000000000000u,
   texcoord_compressed = 0b1000000000000000u
};

constexpr bool marked_as_enum_flag(Vbuf_flags)
{
   return true;
}

constexpr auto vbuf_all_flags_mask =
   Vbuf_flags::position | Vbuf_flags::bone_indices | Vbuf_flags::bone_weights |
   Vbuf_flags::normal | Vbuf_flags::tangents | Vbuf_flags::color |
   Vbuf_flags::static_lighting | Vbuf_flags::texcoords | Vbuf_flags::shadow_data |
   Vbuf_flags::position_compressed | Vbuf_flags::bone_info_compressed |
   Vbuf_flags::normal_compressed | Vbuf_flags::texcoord_compressed;

constexpr auto vbuf_unknown_flags_mask = ~vbuf_all_flags_mask;

constexpr auto vbuf_compressed_mask =
   Vbuf_flags::position_compressed | Vbuf_flags::bone_info_compressed |
   Vbuf_flags::normal_compressed | Vbuf_flags::texcoord_compressed;

struct Vbuf_info {
   std::uint32_t count;
   std::uint32_t stride;
   Vbuf_flags flags;
};

static_assert(std::is_trivially_copyable_v<Vbuf_info>);
static_assert(sizeof(Vbuf_info) == 12);

class Position_decompress {
public:
   Position_decompress(const std::array<glm::vec3, 2> vert_box) noexcept
   {
      low = vert_box[0];
      mul = (vert_box[1] - vert_box[0]);
   }

   glm::vec3 operator()(const glm::i16vec3 compressed) const noexcept
   {
      const auto c = static_cast<glm::vec3>(compressed);
      constexpr float i16min = std::numeric_limits<glm::int16>::min();
      constexpr float i16max = std::numeric_limits<glm::int16>::max();

      return low + (c - i16min) * mul / (i16max - i16min);
   }

private:
   glm::vec3 low;
   glm::vec3 mul;
};

auto select_best_vbuf(const std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs)
   -> Ucfb_reader_strict<"VBUF"_mn>
{
   if (vbufs.empty()) throw std::runtime_error{"modl segm has no VBUFs"};

   std::vector<Ucfb_reader_strict<"VBUF"_mn>> sorted_vbufs;
   sorted_vbufs.reserve(vbufs.size());

   std::copy_if(vbufs.cbegin(), vbufs.cbegin(), std::back_inserter(sorted_vbufs),
                [](Ucfb_reader_strict<"VBUF"_mn> vbuf) {
                   const auto flags = vbuf.read_trivial<Vbuf_info>().flags;

                   return (flags & vbuf_compressed_mask) == Vbuf_flags::none;
                });

   const auto pick_vbuf = [](std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs_to_sort) {
      const auto sort_predicate = [](Ucfb_reader_strict<"VBUF"_mn> left,
                                     Ucfb_reader_strict<"VBUF"_mn> right) {
         const auto lflags = left.read_trivial<Vbuf_info>().flags;
         const auto rflags = right.read_trivial<Vbuf_info>().flags;

         return lflags < rflags;
      };

      std::sort(std::begin(vbufs_to_sort), std::end(vbufs_to_sort), sort_predicate);

      return vbufs_to_sort.back();
   };

   if (!sorted_vbufs.empty()) return pick_vbuf(sorted_vbufs);

   sorted_vbufs.clear();

   std::copy_if(vbufs.cbegin(), vbufs.cbegin(), std::back_inserter(sorted_vbufs),
                [](Ucfb_reader_strict<"VBUF"_mn> vbuf) {
                   const auto flags = vbuf.read_trivial<Vbuf_info>().flags;

                   return (flags & vbuf_compressed_mask) != Vbuf_flags::none;
                });

   if (!sorted_vbufs.empty()) return pick_vbuf(sorted_vbufs);

   return vbufs.back();
}

bool is_pretransformed_vbuf(const Vbuf_flags flags) noexcept
{
   return are_flags_set(flags, Vbuf_flags::bone_indices) &&
          !are_flags_set(flags, Vbuf_flags::bone_weights);
}

auto get_vertices_create_flags(const Vbuf_flags flags) -> model::Vertices::Create_flags
{
   return {.positions = (Vbuf_flags::position & flags) != Vbuf_flags::none,
           .normals = (Vbuf_flags::normal & flags) != Vbuf_flags::none,
           .tangents = (Vbuf_flags::tangents & flags) != Vbuf_flags::none,
           .bitangents = (Vbuf_flags::tangents & flags) != Vbuf_flags::none,
           .colors = ((Vbuf_flags::color & flags) |
                      (Vbuf_flags::static_lighting & flags)) != Vbuf_flags::none,
           .texcoords = (Vbuf_flags::texcoords & flags) != Vbuf_flags::none,
           .bones = (Vbuf_flags::bone_indices & flags) != Vbuf_flags::none,
           .weights = (Vbuf_flags::bone_weights & flags) != Vbuf_flags::none};
}

glm::vec4 read_colour(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto colour = vbuf.read_trivial_unaligned<std::uint32_t>();

   return glm::unpackUnorm4x8(colour).bgra;
}

glm::vec3 read_compressed_normal_pc(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   return (glm::unpackUnorm4x8(vbuf.read_trivial_unaligned<std::uint32_t>()) * 2.0f -
           1.0f)
      .zyx;
}

glm::vec3 read_compressed_normal_xbox(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   constexpr std::array<std::uint32_t, 2> sign_extend_xy = {0x0u, 0xfffffc00u};
   constexpr std::array<std::uint32_t, 2> sign_extend_z = {0x0u, 0xfffff800u};

   const auto dec3_normal = vbuf.read_trivial_unaligned<std::uint32_t>();

   const auto x_unsigned = dec3_normal & 0x7ffu;
   const auto y_unsigned = (dec3_normal >> 11u) & 0x7ffu;
   const auto z_unsigned = (dec3_normal >> 22u) & 0x3ffu;

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

glm::vec3 read_weights(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto weights = vbuf.read_trivial_unaligned<glm::vec2>();

   return glm::vec3{weights.x, weights.y, 1.f - weights.x - weights.y};
}

glm::vec3 read_weights_compressed_pc(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto weights = glm::unpackUnorm4x8(vbuf.read_trivial_unaligned<std::uint32_t>());

   return glm::vec3{weights.z, weights.y, 1.f - weights.z - weights.y};
}

glm::vec3 read_weights_compressed_xbox(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto weights = glm::unpackUnorm4x8(vbuf.read_trivial_unaligned<std::uint16_t>());

   return glm::vec3{weights.x, weights.y, 1.f - weights.x - weights.y};
}

glm::u8vec3 read_bone_indices_pc(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto packed = vbuf.read_trivial<std::uint32_t>();

   glm::u8vec3 indices;

   indices.x = static_cast<std::uint8_t>(packed & 0xffu);
   indices.y = static_cast<std::uint8_t>((packed >> 8u) & 0xffu);
   indices.z = static_cast<std::uint8_t>((packed >> 16u) & 0xffu);

   return indices;
}

glm::vec2 read_compressed_texcoords(Ucfb_reader_strict<"VBUF"_mn>& vbuf)
{
   const auto compressed =
      static_cast<glm::vec2>(vbuf.read_trivial_unaligned<glm::i16vec2>());

   return compressed / 2048.f;
}

void read_vertex_pc(Ucfb_reader_strict<"VBUF"_mn> vbuf, const Vbuf_flags flags,
                    const std::size_t index, const Position_decompress& pos_decompress,
                    model::Vertices& out)
{
   if ((flags & Vbuf_flags::position) == Vbuf_flags::position) {
      if ((flags & Vbuf_flags::position_compressed) == Vbuf_flags::position_compressed) {
         const auto compressed = vbuf.read_trivial_unaligned<glm::i16vec4>();

         out.positions[index] = pos_decompress(compressed);
      }
      else {
         out.positions[index] = vbuf.read_trivial_unaligned<glm::vec3>();
      }
   }

   if ((flags & Vbuf_flags::bone_weights) == Vbuf_flags::bone_weights) {
      if ((flags & Vbuf_flags::bone_info_compressed) ==
          Vbuf_flags::bone_info_compressed) {
         out.weights[index] = read_weights_compressed_pc(vbuf);
      }
      else {
         out.weights[index] = read_weights(vbuf);
      }
   }

   if ((flags & Vbuf_flags::bone_indices) == Vbuf_flags::bone_indices) {
      out.bones[index] = read_bone_indices_pc(vbuf);
   }

   if ((flags & Vbuf_flags::normal) == Vbuf_flags::normal) {
      if ((flags & Vbuf_flags::normal_compressed) == Vbuf_flags::normal_compressed) {
         out.normals[index] = read_compressed_normal_pc(vbuf);
      }
      else {
         out.normals[index] = vbuf.read_trivial_unaligned<glm::vec3>();
      }
   }

   if ((flags & Vbuf_flags::tangents) == Vbuf_flags::tangents) {
      if ((flags & Vbuf_flags::normal_compressed) == Vbuf_flags::normal_compressed) {
         out.bitangents[index] = read_compressed_normal_pc(vbuf);
         out.tangents[index] = read_compressed_normal_pc(vbuf);
      }
      else {
         out.bitangents[index] = vbuf.read_trivial_unaligned<glm::vec3>();
         out.tangents[index] = vbuf.read_trivial_unaligned<glm::vec3>();
      }
   }

   if ((flags & Vbuf_flags::color) == Vbuf_flags::color) {
      out.colors[index] = read_colour(vbuf);
   }

   if ((flags & Vbuf_flags::static_lighting) == Vbuf_flags::static_lighting) {
      out.colors[index] = read_colour(vbuf);
   }

   if ((flags & Vbuf_flags::texcoords) == Vbuf_flags::texcoords) {
      if ((flags & Vbuf_flags::texcoord_compressed) == Vbuf_flags::texcoord_compressed) {
         out.texcoords[index] = read_compressed_texcoords(vbuf);
      }
      else {
         out.texcoords[index] = vbuf.read_trivial_unaligned<glm::vec2>();
      }
   }
}

void read_vertex_xbox(Ucfb_reader_strict<"VBUF"_mn> vbuf, const Vbuf_flags flags,
                      const std::size_t index, const Position_decompress& pos_decompress,
                      model::Vertices& out)
{
   if ((flags & Vbuf_flags::position) == Vbuf_flags::position) {
      if ((flags & Vbuf_flags::position_compressed) == Vbuf_flags::position_compressed) {
         const auto compressed = vbuf.read_trivial_unaligned<glm::i16vec3>();

         out.positions[index] = pos_decompress(compressed);
      }
      else {
         out.positions[index] = vbuf.read_trivial_unaligned<glm::vec3>();
      }
   }

   if ((flags & Vbuf_flags::bone_weights) == Vbuf_flags::bone_weights) {
      if ((flags & Vbuf_flags::bone_info_compressed) ==
          Vbuf_flags::bone_info_compressed) {
         out.weights[index] = read_weights_compressed_xbox(vbuf);
      }
      else {
         out.weights[index] = read_weights(vbuf);
      }
   }

   if ((flags & Vbuf_flags::bone_indices) == Vbuf_flags::bone_indices) {
      if ((flags & Vbuf_flags::bone_weights) == Vbuf_flags::bone_weights) {
         out.bones[index] = vbuf.read_trivial_unaligned<glm::u8vec3>();
      }
      else {
         out.bones[index] = glm::u8vec3{vbuf.read_trivial_unaligned<glm::u8>()};
      }
   }

   if ((flags & Vbuf_flags::normal) == Vbuf_flags::normal) {
      if ((flags & Vbuf_flags::normal_compressed) == Vbuf_flags::normal_compressed) {
         out.normals[index] = read_compressed_normal_xbox(vbuf);
      }
      else {
         out.normals[index] = vbuf.read_trivial_unaligned<glm::vec3>();
      }
   }

   if ((flags & Vbuf_flags::tangents) == Vbuf_flags::tangents) {
      if ((flags & Vbuf_flags::normal_compressed) == Vbuf_flags::normal_compressed) {
         out.bitangents[index] = read_compressed_normal_xbox(vbuf);
         out.tangents[index] = read_compressed_normal_xbox(vbuf);
      }
      else {
         out.bitangents[index] = vbuf.read_trivial_unaligned<glm::vec3>();
         out.tangents[index] = vbuf.read_trivial_unaligned<glm::vec3>();
      }
   }

   if ((flags & Vbuf_flags::color) == Vbuf_flags::color) {
      out.colors[index] = read_colour(vbuf);
   }

   if ((flags & Vbuf_flags::static_lighting) == Vbuf_flags::static_lighting) {
      out.colors[index] = read_colour(vbuf);
   }

   if ((flags & Vbuf_flags::texcoords) == Vbuf_flags::texcoords) {
      if ((flags & Vbuf_flags::texcoord_compressed) == Vbuf_flags::texcoord_compressed) {
         out.texcoords[index] = read_compressed_texcoords(vbuf);
      }
      else {
         out.texcoords[index] = vbuf.read_trivial_unaligned<glm::vec2>();
      }
   }
}

template<auto read_vertex>
void read_vertices(Ucfb_reader_strict<"VBUF"_mn>& vbuf, const Vbuf_info info,
                   const std::array<glm::vec3, 2> vert_box, model::Vertices& out)
{
   const Position_decompress pos_decompress{vert_box};

   for (auto i = 0u; i < info.count; ++i) {
      read_vertex(
         Ucfb_reader_strict<"VBUF"_mn>{vbuf}, // copy reader to emulate stride overlap
         info.flags, i, pos_decompress, out);

      vbuf.consume_unaligned(info.stride);
   }
}

}

auto read_vbuf(const std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs,
               const std::array<glm::vec3, 2> vert_box, const bool xbox)
   -> model::Vertices
{
   auto vbuf = select_best_vbuf(vbufs);

   const auto info = vbuf.read_trivial<Vbuf_info>();

   if ((info.flags & vbuf_unknown_flags_mask) != Vbuf_flags::none) {
      synced_cout::print("VBUF with unknown flags encountered."s, "\n   size : "s,
                         vbuf.size(), "\n   entry count: "s, info.count, "\n   stride: "s,
                         info.stride, "\n   entry flags: "s,
                         to_hexstring(static_cast<std::uint32_t>(info.flags)), '\n');

      throw std::runtime_error{"vbuf with unknown flags"};
   }

   model::Vertices vertices{info.count, get_vertices_create_flags(info.flags)};

   vertices.pretransformed = is_pretransformed_vbuf(info.flags);
   vertices.static_lighting =
      (info.flags & Vbuf_flags::static_lighting) == Vbuf_flags::static_lighting;
   vertices.softskinned =
      (info.flags & Vbuf_flags::bone_weights) == Vbuf_flags::bone_weights;

   try {
      if (xbox) {
         read_vertices<read_vertex_xbox>(vbuf, info, vert_box, vertices);
      }
      else {
         read_vertices<read_vertex_pc>(vbuf, info, vert_box, vertices);
      }
   }
   catch (std::exception&) {
      synced_cout::print(
         "Failed to completely read VBUF. Model may be incomplete or invalid.");
   }

   return vertices;
}
