
#include "bit_flags.hpp"
#include "magic_number.hpp"
#include "math_helpers.hpp"
#include "model_builder.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"
#include "vbuf_reader.hpp"

#include "tbb/task_group.h"

#include <array>
#include <functional>
#include <limits>
#include <string_view>
#include <tuple>
#include <vector>

using namespace std::literals;

namespace {

enum class Primitive_topology_xbox : std::int32_t {
   point_list = 1,
   line_list = 2,
   line_loop = 3,
   line_strip = 4,
   triangle_list = 5,
   triangle_strip = 6,
   triangle_fan = 7,
   quad_list = 8,
   quad_strip = 9,
   polygon = 10
};

enum class Primitive_topology_d3d : std::int32_t {
   point_list = 1,
   line_list = 2,
   line_strip = 3,
   triangle_list = 4,
   triangle_strip = 5,
   triangle_fan = 6
};

enum class Material_flags : std::uint32_t {
   normal = 1,
   hardedged = 2,
   transparent = 4,
   glossmap = 8,
   glow = 16,
   bumpmap = 32,
   additive = 64,
   specular = 128,
   env_map = 256,
   vertex_lighting = 512,
   wireframe = 2048, // Name based off msh flags, may produce some other effect.
   doublesided = 65536,

   scrolling = 16777216,
   energy = 33554432,
   animated = 67108864,

   attached_light = 134217728,
};

enum class Material_flags_swbf1 : std::uint32_t {
   normal = 1,
   hardedged = 2,
   transparent = 4,
   specular = 48,
   additive = 128,
   glow = 256,
   detail = 512,
   scroll = 1024,
   reflection = 4096,
   camouflage = 8192,
   refraction = 16384
};

struct Material_info {
   Material_flags flags;
   std::uint32_t diffuse_colour;
   std::uint32_t specular_colour;
   std::uint32_t specular_exponent;
   std::uint32_t params[2];

   // There is a null-terminated string as the last member declaring the name of the
   // attached-light, this string is always present even if the attached_light flag is
   // unset.
};

static_assert(std::is_pod_v<Material_info>);
static_assert(sizeof(Material_info) == 24);

struct Model_info {

   std::array<glm::vec3, 2> vertex_box;
   std::array<glm::vec3, 2> visibility_box;
   std::uint32_t face_count{};
};

struct Segment_info {
   model::Primitive_topology primitive_topology{};
   std::uint32_t vertex_count{};
   std::uint32_t primitive_count{};
};

auto read_model_name(Ucfb_reader_strict<"NAME"_mn> name)
   -> std::pair<std::string, model::Lod>
{
   const auto name_view = name.read_string();

   const auto suffix = name_view.substr(name_view.length() - 4, 4);
   const auto unsuffixed_name = std::string{name_view.substr(0, name_view.length() - 4)};

   if (suffix == "LOD1"sv) {
      return {unsuffixed_name, model::Lod::one};
   }
   else if (suffix == "LOD2"sv) {
      return {unsuffixed_name, model::Lod::two};
   }
   else if (suffix == "LOD3"sv) {
      return {unsuffixed_name, model::Lod::two};
   }
   else if (suffix == "LOWD"sv) {
      return {unsuffixed_name, model::Lod::lowres};
   }

   return {std::string{name_view}, model::Lod::zero};
}

Model_info read_model_info(Ucfb_reader_strict<"INFO"_mn> info)
{
   const auto size = info.size();

   if (size != 72 && size != 68) {
      throw std::runtime_error{"Unknow model info encountered."};
   }

   // swbfii has an array of four ints vs the array of three for swbf1
   if (size == 72) {
      info.read_trivial<std::array<std::int32_t, 4>>();
   }
   else {
      info.read_trivial<std::array<std::int32_t, 3>>();
   }

   const auto vertex_box = info.read_trivial<std::array<glm::vec3, 2>>();
   const auto vis_box = info.read_trivial<std::array<glm::vec3, 2>>();

   // read unknown int
   info.read_trivial<std::int32_t>();

   const auto face_count = info.read_trivial<std::uint32_t>();

   return {{vertex_box[0], vertex_box[1]}, {vis_box[0], vis_box[1]}, face_count};
}

void read_texture_name(Ucfb_reader_strict<"TNAM"_mn> texture_name,
                       std::array<std::string, 4>& out) noexcept
{
   const auto index = texture_name.read_trivial<std::uint32_t>();
   const auto name = texture_name.read_string();

   if (index < out.size()) {
      out[index] = name;
   }
}

auto read_vertex_strip_ps2(gsl::span<const std::uint16_t> indices, std::int64_t& pos)
   -> std::vector<std::uint16_t>
{
   if (pos + 1 >= indices.size()) throw std::out_of_range{"Index buffer invalid"};

   std::vector<std::uint16_t> strip;
   strip.reserve(32);

   strip.push_back(indices[pos] & ~(0x8000ui16));
   strip.push_back(indices[pos + 1] & ~(0x8000ui16));
   pos += 2;

   for (; pos < indices.size(); ++pos) {
      if ((indices[pos] & 0x8000ui16) == 0x8000ui16) break;

      strip.push_back(indices[pos]);
   }

   return strip;
}

auto read_segment_info_ps2(Ucfb_reader_strict<"INFO"_mn> info) -> Segment_info
{
   const auto [vertex_count, primitive_count] =
      info.read_multi<std::uint32_t, std::uint32_t>();

   return {.primitive_topology = model::Primitive_topology::triangle_strip_ps2,
           .vertex_count = vertex_count,
           .primitive_count = primitive_count};
}

auto read_segment_info_pc(Ucfb_reader_strict<"INFO"_mn> info) -> Segment_info
{
   const auto [primitive_topology_d3d, vertex_count, primitive_count] =
      info.read_multi<Primitive_topology_d3d, std::uint32_t, std::uint32_t>();

   const auto primitive_topology = [primitive_topology_d3d] {
      switch (primitive_topology_d3d) {
      case Primitive_topology_d3d::point_list:
         return model::Primitive_topology::point_list;
      case Primitive_topology_d3d::line_list:
         return model::Primitive_topology::line_list;
      case Primitive_topology_d3d::line_strip:
         return model::Primitive_topology::line_strip;
      case Primitive_topology_d3d::triangle_list:
         return model::Primitive_topology::triangle_list;
      case Primitive_topology_d3d::triangle_strip:
         return model::Primitive_topology::triangle_strip;
      case Primitive_topology_d3d::triangle_fan:
         return model::Primitive_topology::triangle_fan;
      default:
         throw std::runtime_error{"modl segm has unknown primitive topology"};
      }
   }();

   return {.primitive_topology = primitive_topology,
           .vertex_count = vertex_count,
           .primitive_count = primitive_count};
}

auto read_segment_info_xbox(Ucfb_reader_strict<"INFO"_mn> info) -> Segment_info
{
   const auto [primitive_topology_xbox, vertex_count, primitive_count] =
      info.read_multi<Primitive_topology_xbox, std::uint32_t, std::uint32_t>();

   const auto primitive_topology = [primitive_topology_xbox] {
      switch (primitive_topology_xbox) {
      case Primitive_topology_xbox::point_list:
         return model::Primitive_topology::point_list;
      case Primitive_topology_xbox::line_list:
         return model::Primitive_topology::line_list;
      case Primitive_topology_xbox::line_loop:
         return model::Primitive_topology::line_loop;
      case Primitive_topology_xbox::line_strip:
         return model::Primitive_topology::line_strip;
      case Primitive_topology_xbox::triangle_list:
         return model::Primitive_topology::triangle_list;
      case Primitive_topology_xbox::triangle_strip:
         return model::Primitive_topology::triangle_strip;
      case Primitive_topology_xbox::triangle_fan:
         return model::Primitive_topology::triangle_fan;
      case Primitive_topology_xbox::quad_list:
         throw std::runtime_error{
            "modl segm uses unsupported primitive topology \"quad_list\""};
      case Primitive_topology_xbox::quad_strip:
         throw std::runtime_error{
            "modl segm uses unsupported primitive topology \"quad_strip\""};
      case Primitive_topology_xbox::polygon:
         throw std::runtime_error{
            "modl segm uses unsupported primitive topology \"polygon\""};
      default:
         throw std::runtime_error{"modl segm has unknown primitive topology"};
      }
   }();

   return {.primitive_topology = primitive_topology,
           .vertex_count = vertex_count,
           .primitive_count = primitive_count};
}

auto read_index_buffer(Ucfb_reader_strict<"IBUF"_mn> index_buffer)
   -> std::vector<std::uint16_t>
{
   const auto indices_count = index_buffer.read_trivial<std::uint32_t>();

   return index_buffer.read_array<std::uint16_t>(indices_count);
}

auto read_positions_buffer(Ucfb_reader_strict<"POSI"_mn> positions_buffer,
                           const std::uint32_t vertex_count,
                           const std::array<glm::vec3, 2>& vertex_box)
   -> std::unique_ptr<glm::vec3[]>
{
   static_assert(sizeof(glm::u16vec3) == 6);
   const auto compressed_positions =
      positions_buffer.read_array<glm::u16vec3>(vertex_count);

   const std::array<glm::vec3, 2> old_range = {glm::vec3{0.0f}, glm::vec3{65535.0f}};

   auto out_positions = std::make_unique<glm::vec3[]>(vertex_count);

   for (std::size_t i = 0; i < vertex_count; ++i) {
      out_positions[i] = range_convert(static_cast<glm::vec3>(compressed_positions[i]),
                                       old_range, vertex_box);
   }

   return out_positions;
}

auto read_normals_buffer(Ucfb_reader_strict<"NORM"_mn> normals_buffer,
                         const std::uint32_t vertex_count) -> std::unique_ptr<glm::vec3[]>
{
   static_assert(sizeof(glm::i8vec3) == 3);

   const auto compressed_normals = normals_buffer.read_array<glm::i8vec3>(vertex_count);
   auto out_normals = std::make_unique<glm::vec3[]>(vertex_count);

   for (std::size_t i = 0; i < vertex_count; ++i) {
      out_normals[i] = static_cast<glm::vec3>(compressed_normals[i]) / 127.f;
   }

   return out_normals;
}

auto read_uv_buffer(Ucfb_reader_strict<"TEX0"_mn> uv_buffer,
                    const std::uint32_t vertex_count) -> std::unique_ptr<glm::vec2[]>
{
   static_assert(sizeof(glm::i16vec2) == 4);

   const auto compressed_coords = uv_buffer.read_array<glm::i16vec2>(vertex_count);
   auto out_texcoords = std::make_unique<glm::vec2[]>(vertex_count);

   for (std::size_t i = 0; i < vertex_count; ++i) {
      constexpr auto factor = 2048.f;

      out_texcoords[i] = static_cast<glm::vec2>(compressed_coords[i]) / factor;
   }

   return out_texcoords;
}

auto read_skin_buffer(Ucfb_reader_strict<"BONE"_mn> bone_buffer,
                      const std::uint32_t vertex_count) -> std::unique_ptr<glm::u8vec3[]>
{
   const auto hardskin = bone_buffer.read_array<std::uint8_t>(vertex_count);
   auto out_bones = std::make_unique<glm::u8vec3[]>(vertex_count);

   for (std::size_t i = 0; i < vertex_count; ++i) {
      out_bones[i] = glm::u8vec3{hardskin[i]};
   }

   return out_bones;
}

auto read_colour_buffer(Ucfb_reader_strict<"COL0"_mn> uv_buffer,
                        const std::uint32_t vertex_count) -> std::unique_ptr<glm::vec4[]>
{
   const auto packed_colours = uv_buffer.read_array<std::uint32_t>(vertex_count);
   auto out_colours = std::make_unique<glm::vec4[]>(vertex_count);

   for (std::size_t i = 0; i < vertex_count; ++i) {
      out_colours[i] = glm::unpackSnorm4x8(packed_colours[i]).bgra();
   }

   return out_colours;
}

std::vector<std::uint8_t> read_bone_map(Ucfb_reader_strict<"BMAP"_mn> bone_map)
{
   const auto count = bone_map.read_trivial<std::uint32_t>();

   return bone_map.read_array<std::uint8_t>(count);
}

void read_material_swbf1(Ucfb_reader_strict<"MTRL"_mn> material, model::Material& out)
{
   const auto flags = material.read_trivial<Material_flags_swbf1>();

   if (are_flags_set(flags, Material_flags_swbf1::hardedged)) {
      out.flags = set_flags(out.flags, model::Render_flags::hardedged);
   }
   if (are_flags_set(flags, Material_flags_swbf1::transparent)) {
      out.flags = set_flags(out.flags, model::Render_flags::transparent);
   }
   if (are_flags_set(flags, Material_flags_swbf1::specular)) {
      out.type = model::Render_type::specular;
      out.specular_exponent = static_cast<float>(material.read_trivial<std::int32_t>());
      out.specular_colour =
         glm::unpackUnorm4x8(material.read_trivial<std::uint32_t>()).bgra;
   }
   if (are_flags_set(flags, Material_flags_swbf1::additive)) {
      out.flags = set_flags(out.flags, model::Render_flags::additive);
   }
   if (are_flags_set(flags, Material_flags_swbf1::glow)) {
      out.type = model::Render_type::glow;
   }
   if (are_flags_set(flags, Material_flags_swbf1::detail)) {
      out.type = model::Render_type::detail;

      out.params[0] = static_cast<std::uint8_t>(
         range_convert(material.read_trivial<float>(), {-1.0f, 1.0f}, {-128.0f, 127.0f}));
      out.params[1] = static_cast<std::uint8_t>(
         range_convert(material.read_trivial<float>(), {-1.0f, 1.0f}, {-128.0f, 127.0f}));
   }
   if (are_flags_set(flags, Material_flags_swbf1::scroll)) {
      out.type = model::Render_type::scrolling;

      out.params[0] = static_cast<std::uint8_t>(
         range_convert(material.read_trivial<float>(), {-1.0f, 1.0f}, {-128.0f, 127.0f}));
      out.params[1] = static_cast<std::uint8_t>(
         range_convert(material.read_trivial<float>(), {-1.0f, 1.0f}, {-128.0f, 127.0f}));
   }
   if (are_flags_set(flags, Material_flags_swbf1::reflection)) {
      out.type = model::Render_type::reflection;
   }
   if (are_flags_set(flags, Material_flags_swbf1::camouflage)) {
      out.type = model::Render_type::camouflage;
   }
   if (are_flags_set(flags, Material_flags_swbf1::refraction)) {
      out.type = model::Render_type::refraction;
   }
}

void read_material(Ucfb_reader_strict<"MTRL"_mn> material, model::Material& out)
{
   // we can detect swbf1 vs swbf2 material information based off the size of
   // the chunk. swbf1 uses a varying sized chunk that never matches the size
   // of the swbfii one (which is a fixed size, save a trailing string whose purpose I
   // am unsure of).
   if (material.size() < sizeof(Material_info)) {
      return read_material_swbf1(material, out);
   }

   const auto info = material.read_trivial<Material_info>();

   out.diffuse_colour = glm::unpackUnorm4x8(info.diffuse_colour).brga;
   out.specular_colour = glm::unpackUnorm4x8(info.specular_colour).brga;
   out.specular_exponent = static_cast<float>(info.specular_exponent);

   out.params[0] = static_cast<std::uint8_t>(info.params[0]);
   out.params[1] = static_cast<std::uint8_t>(info.params[1]);

   const auto attached_light = material.read_string_unaligned();

   out.vertex_lighting = are_flags_set(info.flags, Material_flags::vertex_lighting);

   if (are_flags_set(info.flags, Material_flags::hardedged)) {
      out.flags = set_flags(out.flags, model::Render_flags::hardedged);
   }
   if (are_flags_set(info.flags, Material_flags::transparent) &&
       !are_flags_set(info.flags, Material_flags::doublesided)) {
      out.flags = set_flags(out.flags, model::Render_flags::transparent);
   }
   if (are_flags_set(info.flags, Material_flags::glow)) {
      out.flags = set_flags(out.flags, model::Render_flags::glow);
   }
   if (are_flags_set(info.flags, Material_flags::bumpmap)) {
      out.type = model::Render_type::bumpmap;
   }
   if (are_flags_set(info.flags, Material_flags::additive)) {
      out.flags = set_flags(out.flags, model::Render_flags::additive);
   }
   if (are_flags_set(info.flags, Material_flags::specular)) {
      out.flags = set_flags(out.flags, model::Render_flags::specular);
   }
   if (are_flags_set(info.flags, Material_flags::env_map)) {
      out.type = model::Render_type::env_map;
   }
   if (are_flags_set(info.flags, Material_flags::wireframe)) {
      out.type = model::Render_type::wireframe;
   }
   if (are_flags_set(info.flags, Material_flags::doublesided)) {
      out.flags = set_flags(out.flags, model::Render_flags::doublesided);
   }
   if (are_flags_set(info.flags, Material_flags::scrolling)) {
      out.type = model::Render_type::scrolling;
   }
   if (are_flags_set(info.flags, Material_flags::energy)) {
      out.type = model::Render_type::energy;
   }
   if (are_flags_set(info.flags, Material_flags::animated)) {
      out.type = model::Render_type::animated;
   }
   if (are_flags_set(info.flags, Material_flags::attached_light)) {
      out.attached_light = attached_light;
   }
}

void read_material_name(Ucfb_reader_strict<"MNAM"_mn> mnam, model::Part& out)
{
   const auto name = mnam.read_string();

   out.material.name = name;
   out.name = name;
}

void read_render_type(Ucfb_reader_strict<"RTYP"_mn> render_type, model::Material& out)
{
   const auto type = render_type.read_string();

   if (type == "Refraction"sv) {
      out.type = model::Render_type::refraction;
   }
   else if (type == "Bump"sv) {
      if (out.type == model::Render_type::specular) {
         out.type = model::Render_type::bumpmap_specular;
      }
      else {
         out.type = model::Render_type::bumpmap;
      }
   }
   else if (type == "Water"sv) {
      out.type = model::Render_type::water;
   }
}

auto process_segment_pc_xbox(Ucfb_reader_strict<"segm"_mn> segment, const Model_info info,
                             const model::Lod lod, const bool xbox) -> model::Part
{
   model::Part part{.lod = lod};

   std::vector<Ucfb_reader_strict<"VBUF"_mn>> vbufs;
   vbufs.reserve(8);

   while (segment) {
      const auto child = segment.read_child();

      if (child.magic_number() == "INFO"_mn) {
         part.primitive_topology =
            (xbox ? read_segment_info_xbox(Ucfb_reader_strict<"INFO"_mn>{child})
                  : read_segment_info_pc(Ucfb_reader_strict<"INFO"_mn>{child}))
               .primitive_topology;
      }
      else if (child.magic_number() == "MTRL"_mn) {
         read_material(Ucfb_reader_strict<"MTRL"_mn>{child}, part.material);
      }
      else if (child.magic_number() == "RTYP"_mn) {
         read_render_type(Ucfb_reader_strict<"RTYP"_mn>{child}, part.material);
      }
      else if (child.magic_number() == "MNAM"_mn) {
         read_material_name(Ucfb_reader_strict<"MNAM"_mn>{child}, part);
      }
      else if (child.magic_number() == "TNAM"_mn) {
         read_texture_name(Ucfb_reader_strict<"TNAM"_mn>{child}, part.material.textures);
      }
      else if (child.magic_number() == "IBUF"_mn) {
         part.indices = read_index_buffer(Ucfb_reader_strict<"IBUF"_mn>{child});
      }
      else if (child.magic_number() == "VBUF"_mn) {
         vbufs.emplace_back(Ucfb_reader_strict<"VBUF"_mn>{child});
      }
      else if (child.magic_number() == "BNAM"_mn) {
         part.parent = Ucfb_reader_strict<"BNAM"_mn>{child}.read_string();
      }
      else if (child.magic_number() == "BMAP"_mn) {
         part.bone_map = read_bone_map(Ucfb_reader_strict<"BMAP"_mn>{child});
      }
   }

   part.vertices = read_vbuf(vbufs, info.vertex_box, xbox);

   return part;
}

auto process_segment_ps2(Ucfb_reader_strict<"segm"_mn> segment, const Model_info info,
                         const model::Lod lod) -> model::Part
{
   model::Part part{.lod = lod};

   const auto [ignore, vertex_count, index_count] =
      read_segment_info_ps2(segment.read_child_strict<"INFO"_mn>());

   part.vertices = model::Vertices{vertex_count, {}};

   while (segment) {
      const auto child = segment.read_child();

      if (child.magic_number() == "MTRL"_mn) {
         read_material(Ucfb_reader_strict<"MTRL"_mn>{child}, part.material);
      }
      else if (child.magic_number() == "RTYP"_mn) {
         auto rtyp = Ucfb_reader_strict<"RTYP"_mn>{child};

         part.material.type =
            static_cast<model::Render_type>(rtyp.read_trivial<std::uint32_t>());
      }
      else if (child.magic_number() == "MNAM"_mn) {
         read_material_name(Ucfb_reader_strict<"MNAM"_mn>{child}, part);
      }
      else if (child.magic_number() == "TNAM"_mn) {
         read_texture_name(Ucfb_reader_strict<"TNAM"_mn>{child}, part.material.textures);
      }
      else if (child.magic_number() == "STRP"_mn) {
         part.indices =
            Ucfb_reader_strict<"STRP"_mn>{child}.read_array<std::uint16_t>(index_count);
      }
      else if (child.magic_number() == "POSI"_mn) {
         part.vertices.positions = read_positions_buffer(
            Ucfb_reader_strict<"POSI"_mn>{child}, vertex_count, info.vertex_box);
      }
      else if (child.magic_number() == "NORM"_mn) {
         part.vertices.normals =
            read_normals_buffer(Ucfb_reader_strict<"NORM"_mn>{child}, vertex_count);
      }
      else if (child.magic_number() == "TEX0"_mn) {
         part.vertices.texcoords =
            read_uv_buffer(Ucfb_reader_strict<"TEX0"_mn>{child}, vertex_count);
      }
      else if (child.magic_number() == "COL0"_mn) {
         part.vertices.colors =
            read_colour_buffer(Ucfb_reader_strict<"COL0"_mn>{child}, vertex_count);
      }
      else if (child.magic_number() == "BMAP"_mn) {
         part.bone_map = read_bone_map(Ucfb_reader_strict<"BMAP"_mn>{child});
         part.vertices.pretransformed = true;
      }
      else if (child.magic_number() == "BONE"_mn) {
         part.vertices.bones =
            read_skin_buffer(Ucfb_reader_strict<"BONE"_mn>{child}, vertex_count);
      }
      else if (child.magic_number() == "BNAM"_mn) {
         part.parent = Ucfb_reader_strict<"BNAM"_mn>{child}.read_string();
      }
   }

   return part;
}

template<typename Segm_processor>
auto handle_model_impl(Segm_processor&& segm_processor, Ucfb_reader model) -> model::Model
{
   auto [name, lod] = read_model_name(model.read_child_strict<"NAME"_mn>());

   model.read_child_strict_optional<"VRTX"_mn>();

   model.read_child_strict<"NODE"_mn>();
   const auto model_info = read_model_info(model.read_child_strict<"INFO"_mn>());

   model::Model result{.name = name};

   result.parts.reserve(16); // Reserve enough space for all but the most complex models.

   while (model) {
      const auto child = model.read_child();

      if (child.magic_number() == "segm"_mn) {
         result.parts.emplace_back(
            std::invoke(std::forward<Segm_processor>(segm_processor),
                        Ucfb_reader_strict<"segm"_mn>{child}, model_info, lod));
      }
   }

   return result;
}
}

void handle_model(Ucfb_reader model, model::Models_builder& builders)
{
   builders.integrate(handle_model_impl(
      [](auto... args) { return process_segment_pc_xbox(args..., false); }, model));
}

void handle_model_xbox(Ucfb_reader model, model::Models_builder& builders)
{
   builders.integrate(handle_model_impl(
      [](auto... args) { return process_segment_pc_xbox(args..., true); }, model));
}

void handle_model_ps2(Ucfb_reader model, model::Models_builder& builders)
{
   builders.integrate(handle_model_impl(process_segment_ps2, model));
}
