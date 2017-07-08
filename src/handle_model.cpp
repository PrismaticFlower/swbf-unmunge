
#include "bit_flags.hpp"
#include "chunk_headers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"
#include "vbuf_helpers.hpp"

#include "tbb/task_group.h"

#define GLM_FORCE_CXX98
#include "glm/vec3.hpp"

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

#pragma pack(push, 1)

struct Render_type {
   Magic_number mn;
   std::uint32_t size;

   char str[];
};

static_assert(std::is_standard_layout_v<Render_type>);
static_assert(sizeof(Render_type) == 8);

struct Tex_name {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t index;

   char str[];
};

static_assert(std::is_standard_layout_v<Tex_name>);
static_assert(sizeof(Tex_name) == 12);

struct Bone_name {
   Magic_number mn;
   std::uint32_t size;

   char str[];
};

static_assert(std::is_standard_layout_v<Bone_name>);
static_assert(sizeof(Bone_name) == 8);

struct Ibuf {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t entry_count;

   std::uint16_t indices[];
};

static_assert(std::is_standard_layout_v<Ibuf>);
static_assert(sizeof(Ibuf) == 12);

struct Material {
   Magic_number mn;
   std::uint32_t size;

   Material_flags flags;
   std::uint32_t unknown_1;
   std::array<std::uint8_t, 4> colour;
   std::uint32_t specular_intensity;
   std::uint32_t params[2];

   char unknown_str[1];

   static_assert(std::is_standard_layout_v<std::array<std::uint8_t, 4>>);
   static_assert(sizeof(std::array<std::uint8_t, 4>) == 4);
};

static_assert(std::is_standard_layout_v<Material>);
static_assert(sizeof(Material) == 33);

struct Bonemap {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t count;

   std::uint8_t bones[];
};

static_assert(std::is_standard_layout_v<Bonemap>);
static_assert(sizeof(Bonemap) == 12);

struct Shadow_vertices {
   Magic_number mn;
   std::uint32_t size;

   glm::vec3 vertices[];

   static_assert(std::is_standard_layout_v<glm::vec3>);
   static_assert(sizeof(glm::vec3) == 12);
};

static_assert(std::is_standard_layout_v<Shadow_vertices>);
static_assert(sizeof(Shadow_vertices) == 8);

struct Shadow_indices {
   Magic_number mn;
   std::uint32_t size;

   std::uint16_t indices[][4];

   static_assert(std::is_standard_layout_v<std::uint16_t[4]>);
   static_assert(sizeof(std::uint16_t[4]) == 8);
};

static_assert(std::is_standard_layout_v<Shadow_indices>);
static_assert(sizeof(Shadow_indices) == 8);

struct Shadow_skin {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t count;
   std::uint32_t unknown_1;

   struct Skin_info {
      std::uint16_t marker;
      std::uint8_t bone;
      std::uint8_t unused[2];
   };

   static_assert(std::is_standard_layout_v<Skin_info>);
   static_assert(sizeof(Skin_info) == 5);

   Skin_info skin[];
};

static_assert(std::is_standard_layout_v<Shadow_skin>);
static_assert(sizeof(Shadow_skin) == 16);

struct Cshadow {
   Magic_number mn;
   std::uint32_t size;

   Byte info_bytes[12];

   std::uint32_t data_mn;
   std::uint32_t data_size;

   Byte data[];
};

static_assert(std::is_standard_layout_v<Cshadow>);
static_assert(sizeof(Cshadow) == 28);

struct Segment {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Segment>);
static_assert(sizeof(Segment) == 8);

struct Shadow {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Shadow>);
static_assert(sizeof(Shadow) == 8);

struct Model_info {
   Magic_number mn;
   std::uint32_t size;

   std::uint32_t unknown_1[4];

   glm::vec3
      bbox_pos; // Maybe? I'm not entirely sure, but whatever it is we don't need it.
   glm::vec3 bbox_extent;
   float unused[6];

   std::uint32_t unknown_2[2];
};

static_assert(std::is_standard_layout_v<Model_info>);
static_assert(sizeof(Model_info) == 80);

#pragma pack(pop)

const Model_info* find_model_info(const chunks::Model& model)
{
   std::uint32_t head = model.name_size;
   const std::uint32_t end = model.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };
   align_head();

   std::vector<const Segment*> segments;
   segments.reserve(4);

   while (head < end) {
      const auto& info = view_type_as<Model_info>(model.bytes[head]);

      if (info.mn == "INFO"_mn) return &info;

      head += info.size + 8;

      if (head % 4 != 0) head += (4 - (head % 4));
   }

   return nullptr;
}

std::vector<const Segment*> find_segments(const chunks::Model& model)
{
   std::uint32_t head = model.name_size;
   const std::uint32_t end = model.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };
   align_head();

   std::vector<const Segment*> segments;
   segments.reserve(4);

   while (head < end) {
      const auto& segm = view_type_as<Segment>(model.bytes[head]);

      if (segm.mn == "segm"_mn) segments.emplace_back(&segm);

      head += segm.size + 8;

      if (head % 4 != 0) head += (4 - (head % 4));
   }

   return segments;
}

std::vector<const Shadow*> find_shadows(const chunks::Model& model)
{
   std::uint32_t head = model.name_size;
   const std::uint32_t end = model.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };
   align_head();

   std::vector<const Shadow*> segments;
   segments.reserve(4);

   while (head < end) {
      const auto& segm = view_type_as<Shadow>(model.bytes[head]);

      if (segm.mn == "shdw"_mn) segments.emplace_back(&segm);

      head += segm.size + 8;

      if (head % 4 != 0) head += (4 - (head % 4));
   }

   return segments;
}

std::string_view read_texture_name(const Tex_name& tnam) noexcept
{
   return {&tnam.str[0], tnam.size - 5};
}

std::string_view read_bone_name(const Bone_name& bnam) noexcept
{
   return {&bnam.str[0], bnam.size - 1};
}

auto read_vertex_strip(const Ibuf& ibuf, std::size_t& pos) -> std::vector<std::uint16_t>
{
   if (pos + 2 >= ibuf.entry_count) throw std::out_of_range{"Index buffer invalid"};

   std::vector<std::uint16_t> strip;
   strip.reserve(32);

   std::uint16_t last_index = 0xFFFF;

   for (; pos < ibuf.entry_count; ++pos) {
      if (ibuf.indices[pos] == last_index) {
         ++pos;
         break;
      }

      strip.push_back(ibuf.indices[pos]);
   }

   if (ibuf.indices[pos] == ibuf.indices[pos + 1]) ++pos;

   return strip;
}

auto read_index_buffer(const Ibuf& ibuf) -> std::vector<std::vector<std::uint16_t>>
{
   std::vector<std::vector<std::uint16_t>> strips;
   strips.reserve(32);

   for (std::size_t i = 0; i < ibuf.entry_count; ++i) {
      strips.emplace_back(read_vertex_strip(ibuf, i));
   }

   std::size_t size = 0;

   for (const auto& s : strips) {
      size += s.size();
   }

   return strips;
}

std::vector<std::uint8_t> read_bone_map(const Bonemap& bmap)
{
   std::vector<std::uint8_t> result;
   result.reserve(bmap.count);

   for (std::size_t i = 0; i < bmap.count; ++i) {
      result.emplace_back(bmap.bones[i]);
   }

   return result;
}

std::vector<glm::vec3> read_shadow_vertices(const Shadow_vertices& shdv)
{
   std::vector<glm::vec3> vertices{shdv.size / sizeof(glm::vec3)};

   std::memcpy(vertices.data(), &shdv.vertices[0], vertices.size() * sizeof(glm::vec3));

   return vertices;
}

auto read_shadow_indices(const Shadow_indices& shdi)
   -> std::vector<std::vector<std::uint16_t>>
{
   const auto count = shdi.size / sizeof(shdi.indices[0]);

   std::vector<std::vector<std::uint16_t>> strips;
   strips.reserve(count);

   for (std::size_t i = 0; i < count; ++i) {
      strips.emplace_back(std::initializer_list<std::uint16_t>{
         shdi.indices[i][0], shdi.indices[i][1], shdi.indices[i][2]});
   }

   return strips;
}

std::vector<std::uint8_t> read_shadow_skin(const Shadow_skin& skin)
{
   std::vector<std::uint8_t> result;
   result.reserve(skin.count);

   for (std::size_t i = 0; i < skin.count; ++i) {
      result.emplace_back(skin.skin[i].bone);
   }

   return result;
}

void read_material(const Material& mat, msh::Material& out)
{
   out.colour = {mat.colour[0] / 255.0f, mat.colour[1] / 255.0f, mat.colour[2] / 255.0f,
                 mat.colour[3] / 255.0f};

   out.specular_value = static_cast<float>(mat.specular_intensity);

   out.params[0] = static_cast<std::uint8_t>(mat.params[0]);
   out.params[1] = static_cast<std::uint8_t>(mat.params[1]);

   if (are_flags_set(mat.flags, Material_flags::hardedged)) {
      out.flags = set_flags(out.flags, msh::Render_flags::hardedged);
   }
   if (are_flags_set(mat.flags, Material_flags::hardedged)) {
      out.flags = set_flags(out.flags, msh::Render_flags::doublesided);
   }
   if (are_flags_set(mat.flags, Material_flags::singlesided)) {
      out.flags = set_flags(out.flags, msh::Render_flags::singlesided);
   }
   if (are_flags_set(mat.flags, Material_flags::glow)) {
      out.flags = set_flags(out.flags, msh::Render_flags::glow);
   }
   if (are_flags_set(mat.flags, Material_flags::bumpmap)) {
      out.type = msh::Render_type::bumpmap;
   }
   if (are_flags_set(mat.flags, Material_flags::additive)) {
      out.flags = set_flags(out.flags, msh::Render_flags::additive);
   }
   if (are_flags_set(mat.flags, Material_flags::specular)) {
      out.flags = set_flags(out.flags, msh::Render_flags::specular);
   }
   if (are_flags_set(mat.flags, Material_flags::env_map)) {
      out.type = msh::Render_type::env_map;
   }
   if (are_flags_set(mat.flags, Material_flags::wireframe)) {
      out.type = msh::Render_type::wireframe;
   }
   if (are_flags_set(mat.flags, Material_flags::doublesided)) {
      out.flags = set_flags(out.flags, msh::Render_flags::doublesided);
   }
   if (are_flags_set(mat.flags, Material_flags::scrolling)) {
      out.type = msh::Render_type::scrolling;
   }
   if (are_flags_set(mat.flags, Material_flags::energy)) {
      out.type = msh::Render_type::energy;
   }
   if (are_flags_set(mat.flags, Material_flags::animated)) {
      out.type = msh::Render_type::animated;
   }
}

void read_render_type(const Render_type& rtyp, msh::Material& out)
{
   const std::string_view type{&rtyp.str[0], rtyp.size - 1};

   if (type == "Refraction"_sv) {
      out.type = msh::Render_type::refraction;
   }
}

void read_cshadow(const Cshadow& cshd, msh::Shadow& out)
{
   std::uint32_t head = 0;
   const std::uint32_t end = cshd.size;

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(cshd.data[head]);

      if (chunk.mn == "SHDV"_mn) {
         out.vertices =
            read_shadow_vertices(view_type_as<Shadow_vertices>(cshd.data[head]));
      }
      else if (chunk.mn == "SHDI"_mn) {
         out.strips = read_shadow_indices(view_type_as<Shadow_indices>(cshd.data[head]));
      }
      else if (chunk.mn == "SKIN"_mn) {
         out.skin = read_shadow_skin(view_type_as<Shadow_skin>(cshd.data[head]));
      }

      head += chunk.size + 8;

      if (head % 4 != 0) head += (4 - (head % 4));
   }
}

void process_segment(const Segment& segm, msh::Builder& builder)
{
   std::uint32_t head = 0;
   const std::uint32_t end = segm.size;

   msh::Model model{};

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(segm.bytes[head]);

      if (chunk.mn == "MTRL"_mn) {
         read_material(view_type_as<Material>(segm.bytes[head]), model.material);
      }
      else if (chunk.mn == "RTYP"_mn) {
         read_render_type(view_type_as<Render_type>(segm.bytes[head]), model.material);
      }
      else if (chunk.mn == "TNAM"_mn) {
         const auto name = read_texture_name(view_type_as<Tex_name>(segm.bytes[head]));
         model.material.textures.emplace_back(name);
      }
      else if (chunk.mn == "IBUF"_mn) {
         model.strips = read_index_buffer(view_type_as<Ibuf>(segm.bytes[head]));
      }
      else if (chunk.mn == "VBUF"_mn) {
         process_vbuf(view_type_as<Vbuf>(segm.bytes[head]), model);
      }
      else if (chunk.mn == "BNAM"_mn) {
         model.parent = read_bone_name(view_type_as<Bone_name>(segm.bytes[head]));
      }
      else if (chunk.mn == "BMAP"_mn) {
         model.bone_map = read_bone_map(view_type_as<Bonemap>(segm.bytes[head]));
      }

      head += chunk.size + 8;

      if (head % 4 != 0) head += (4 - (head % 4));
   }

   builder.add_model(std::move(model));
}

void process_segments(const chunks::Model& model, msh::Builder& builder)
{
   const auto segments = find_segments(model);

   for (const auto segment : segments) {
      process_segment(*segment, builder);
   }
}

void process_shadow(const Shadow& shdw, msh::Builder& builder)
{
   std::uint32_t head = 0;
   const std::uint32_t end = shdw.size;

   msh::Shadow shadow{};

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(shdw.bytes[head]);

      if (chunk.mn == "CSHD"_mn) {
         read_cshadow(view_type_as<Cshadow>(shdw.bytes[head]), shadow);
      }
      else if (chunk.mn == "BNAM"_mn) {
         shadow.parent = read_bone_name(view_type_as<Bone_name>(shdw.bytes[head]));
      }
      else if (chunk.mn == "BMAP"_mn) {
         shadow.bone_map = read_bone_map(view_type_as<Bonemap>(shdw.bytes[head]));
      }

      head += chunk.size + 8;

      if (head % 4 != 0) head += (4 - (head % 4));
   }

   builder.add_shadow(std::move(shadow));
}

void process_shadows(const chunks::Model& model, msh::Builder& builder)
{
   const auto shadows = find_shadows(model);

   for (const auto shadow : shadows) {
      process_shadow(*shadow, builder);
   }
}
}

void handle_model(const chunks::Model& model, msh::Builders_map& builders,
                  tbb::task_group& tasks)
{
   const std::string name{reinterpret_cast<const char*>(&model.bytes[0]),
                          model.name_size - 1};

   auto& builder = builders[name];

   tasks.run([&builder, &model] { process_segments(model, builder); });
   tasks.run([&builder, &model] { process_shadows(model, builder); });

   const auto* info = find_model_info(model);

   if (info) {
      builder.set_bbox_extent(info->bbox_extent);
   }
}
