
#include "model_gltf_save.hpp"
#include "file_saver.hpp"
#include "model_topology_converter.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include "fx/gltf.h"
#include <fmt/format.h>
#include <gsl/gsl>

using namespace std::literals;

namespace model::gltf {

namespace {

constexpr bool GLTF_EXPORT_SKIN = false; // todo: Implement glTF skin support.

auto unstripfy_scene_nodes_topologies(std::vector<scene::Node>& nodes)
{
   for (auto& node : nodes) {
      if (node.geometry) {
         switch (node.geometry->topology) {
            // Not sure why but blender doesn't seam to like the triangle strips from the
            // game's models, so we convert them to lists here.
         case Primitive_topology::triangle_strip:
         case Primitive_topology::triangle_strip_ps2:
            node.geometry->indices =
               convert_topology(node.geometry->indices, node.geometry->topology,
                                Primitive_topology::triangle_list);
            node.geometry->topology = Primitive_topology::triangle_list;
            break;
         }
      }
   }
}

auto map_primitive_topology(const Primitive_topology primitive_topology)
   -> fx::gltf::Primitive::Mode
{
   using Mode = fx::gltf::Primitive::Mode;

   switch (primitive_topology) {
   case Primitive_topology::point_list:
      return Mode::Points;
   case Primitive_topology::line_list:
      return Mode::Lines;
   case Primitive_topology::line_loop:
      return Mode::LineLoop;
   case Primitive_topology::line_strip:
      return Mode::LineStrip;
   case Primitive_topology::triangle_list:
      return Mode::Triangles;
   case Primitive_topology::triangle_strip:
      return Mode::TriangleStrip;
   case Primitive_topology::triangle_fan:
      return Mode::TriangleFan;
   case Primitive_topology::undefined:
   case Primitive_topology::triangle_strip_ps2:
      throw std::runtime_error{"Attempt to save model to glTF format that contains "
                               "unsupported primitive topology."};
   default:
      std::terminate();
   }
}

auto align_buffer(std::vector<std::uint8_t>& buffer, const std::size_t multiple)
{
   const auto size = buffer.size();
   const auto remainder = size % multiple;

   if (remainder != 0) buffer.resize(size + (multiple - remainder));
}

auto make_node_matrix(const glm::mat4x3 matrix) noexcept -> std::array<float, 16>
{
   const glm::mat4 mat4x4 = matrix;
   std::array<float, 16> dest;

   std::memcpy(dest.data(), &mat4x4, sizeof(glm::mat4));

   return dest;
}

auto make_node_children_list(const std::string_view name,
                             const std::vector<scene::Node>& nodes)
   -> std::vector<std::int32_t>
{
   std::vector<std::int32_t> children;
   children.reserve(32);

   for (std::int32_t i = 0; i < nodes.size(); ++i) {
      if (nodes[i].parent == name) children.push_back(i);
   }

   return children;
}

auto make_gltf_tangents(const Vertices& vertices) -> std::unique_ptr<glm::vec4[]>
{
   if (!vertices.normals || !vertices.tangents || vertices.bitangents) {
      return nullptr;
   }

   auto packed = std::make_unique<glm::vec4[]>(vertices.size);

   for (std::size_t i = 0; i < vertices.size; ++i) {
      packed[i] = {
         vertices.tangents[i],
         glm::sign(glm::dot(vertices.bitangents[i],
                            glm::cross(vertices.normals[i], vertices.tangents[i])))};
   }

   return packed;
}

auto make_extended_bone_indices(const Vertices& vertices)
   -> std::unique_ptr<glm::u8vec4[]>
{
   if (!vertices.bones) return nullptr;

   auto extended = std::make_unique<glm::u8vec4[]>(vertices.size);

   std::transform(vertices.bones.get(), vertices.bones.get() + vertices.size,
                  extended.get(), [](const glm::u8vec3 v) {
                     return glm::u8vec4{v, 0};
                  });

   return extended;
}

auto make_normalized_bone_weights(const Vertices& vertices)
   -> std::unique_ptr<glm::vec4[]>
{
   if (!vertices.weights) return nullptr;

   auto normalized = std::make_unique<glm::vec4[]>(vertices.size);

   std::transform(vertices.weights.get(), vertices.weights.get() + vertices.size,
                  normalized.get(), [](const glm::vec3 v) {
                     const auto total = v.x + v.y + v.z;

                     return glm::u8vec4{v / total, 0};
                  });

   return normalized;
}

template<typename T>
auto add_to_buffer(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                   const gsl::span<const T> span) -> std::int32_t
{
   align_buffer(buffer, sizeof(T));

   const auto offset = buffer.size();
   const auto span_bytes_size = (span.size() * sizeof(T));

   buffer.resize(buffer.size() + span_bytes_size);

   std::memcpy(buffer.data() + offset, span.data(), span_bytes_size);

   const auto view_index = doc.bufferViews.size();
   doc.bufferViews.push_back({.buffer = 0,
                              .byteOffset = static_cast<std::uint32_t>(offset),
                              .byteLength = static_cast<std::uint32_t>(span_bytes_size)});

   return static_cast<std::int32_t>(view_index);
}

auto add_primitve_indices(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                          const Indices& indices) -> std::int32_t
{
   const auto accessor_index = static_cast<std::int32_t>(doc.accessors.size());

   doc.accessors.push_back(
      {.bufferView = add_to_buffer(doc, buffer, gsl::make_span(indices)),
       .count = static_cast<std::uint32_t>(indices.size()),
       .componentType = fx::gltf::Accessor::ComponentType::UnsignedShort,
       .type = fx::gltf::Accessor::Type::Scalar});

   return accessor_index;
}

template<typename T>
auto add_attribute_accessor(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                            const gsl::span<const T> span,
                            [[maybe_unused]] const bool add_min_max = false)
   -> std::uint32_t
{
   constexpr auto type = []() {
      if (T::length() == 2) return fx::gltf::Accessor::Type::Vec2;
      if (T::length() == 3) return fx::gltf::Accessor::Type::Vec3;
      if (T::length() == 4) return fx::gltf::Accessor::Type::Vec4;
   }();

   constexpr auto component_type = []() {
      if (std::is_same_v<T::value_type, float>)
         return fx::gltf::Accessor::ComponentType::Float;
      if (std::is_same_v<T::value_type, glm::uint8>)
         return fx::gltf::Accessor::ComponentType::UnsignedByte;
   }();

   const auto accessor_index = doc.accessors.size();

   doc.accessors.push_back({.bufferView = add_to_buffer(doc, buffer, span),
                            .count = static_cast<std::uint32_t>(span.size()),
                            .componentType = component_type,
                            .type = type});

   if constexpr (std::is_same_v<typename T::value_type, float>) {
      if (add_min_max) {
         auto& min = doc.accessors.back().min;
         auto& max = doc.accessors.back().max;
         min.assign(T::length(), std::numeric_limits<float>::max());
         max.assign(T::length(), std::numeric_limits<float>::lowest());

         for (auto& v : span) {
            for (auto i = 0; i < T::length(); ++i) {
               min[i] = std::min(min[i], v[i]);
               max[i] = std::max(max[i], v[i]);
            }
         }
      }
   }

   return static_cast<std::uint32_t>(accessor_index);
}

auto add_primitve_attributes(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                             const Vertices& vertices) -> fx::gltf::Attributes
{
   fx::gltf::Attributes attribs;

   const auto add_attrib = [&](const std::string& name, const auto* const data,
                               const bool add_min_max = false) {
      if (!data) return;

      attribs[name] = add_attribute_accessor(
         doc, buffer, gsl::make_span(data, vertices.size), add_min_max);
   };

   add_attrib("POSITION"s, vertices.positions.get(), true);
   add_attrib("NORMAL"s, vertices.normals.get());
   add_attrib("TANGENT"s, make_gltf_tangents(vertices).get());
   add_attrib("TEXCOORD_0"s, vertices.texcoords.get());
   add_attrib("COLOR_0"s, vertices.colors.get());

   if constexpr (GLTF_EXPORT_SKIN) {
      add_attrib("JOINTS_0"s, make_extended_bone_indices(vertices).get());

      if (vertices.bones && vertices.weights) {
         add_attrib("WEIGHTS_0"s, make_normalized_bone_weights(vertices).get());
      }
      else if (vertices.bones) {
         add_attrib(
            "WEIGHTS_0"s,
            std::vector<glm::vec4>{vertices.size, glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}}
               .data());
      }
   }

   return attribs;
}

auto add_mesh_primitve(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                       const scene::Geometry& geometry, const std::int32_t material_index)
   -> fx::gltf::Primitive
{
   return {.indices = add_primitve_indices(doc, buffer, geometry.indices),
           .material = material_index,
           .mode = map_primitive_topology(geometry.topology),
           .attributes = add_primitve_attributes(doc, buffer, geometry.vertices)};
}

auto add_node_mesh(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                   const scene::Node& node) -> std::int32_t
{
   if (!node.geometry) return -1;

   const auto index = static_cast<std::int32_t>(doc.meshes.size());

   doc.meshes.push_back({.name = node.name,
                         .primitives = {add_mesh_primitve(doc, buffer, *node.geometry,
                                                          node.material_index)}});

   return index;
}

auto add_texture_image(fx::gltf::Document& doc, const std::string_view name)
   -> std::int32_t
{
   const auto index = static_cast<std::int32_t>(doc.images.size());

   doc.images.push_back(
      {.name = std::string{name}, .uri = fmt::format("./{}.png"sv, name)});

   return index;
}

auto add_material_texture(fx::gltf::Document& doc, const std::string_view name)
   -> std::int32_t
{
   const auto index = static_cast<std::int32_t>(doc.textures.size());

   doc.textures.push_back(
      {.name = std::string{name}, .source = add_texture_image(doc, name)});

   return index;
}

auto add_material_normal_texture(fx::gltf::Document& doc, const scene::Material& material)
   -> fx::gltf::Material::NormalTexture
{
   if ((material.rendertype != Render_type::bumpmap &&
        material.rendertype != Render_type::bumpmap_specular) ||
       material.textures[1].empty()) {
      return {};
   }

   fx::gltf::Material::NormalTexture tex;

   tex.index = add_material_texture(doc, material.textures[1]);

   return tex;
}

auto add_material_pbr(fx::gltf::Document& doc, const scene::Material& material)
   -> fx::gltf::Material::PBRMetallicRoughness
{
   fx::gltf::Material::PBRMetallicRoughness pbr;

   pbr.baseColorFactor = {material.diffuse_colour[0], material.diffuse_colour[1],
                          material.diffuse_colour[2], material.diffuse_colour[3]};

   if (!material.textures[0].empty()) {
      pbr.baseColorTexture.index = add_material_texture(doc, material.textures[0]);
   }

   // Guess with extreme disregard for reality what the roughness and metallic factors
   // should be.
   if (const float spec_strength =
          glm::clamp(glm::dot(glm::vec3{material.specular_colour},
                              glm::vec3{0.2126f, 0.7152f, 0.0722f}),
                     0.0f, 1.0f);
       are_flags_set(material.flags, Render_flags::specular) ||
       material.rendertype == Render_type::specular ||
       material.rendertype == Render_type::bumpmap_specular) {
      pbr.roughnessFactor = 1.0f - ((1.0f - 0.4f) * spec_strength);
   }
   else if (material.rendertype == Render_type::env_map) {
      pbr.roughnessFactor = 1.0f - spec_strength;
      pbr.metallicFactor = 1.0f - ((1.0f - 0.4f) * spec_strength);
   }

   return pbr;
}

auto add_material(fx::gltf::Document& doc, const scene::Material& material)
   -> fx::gltf::Material

{
   return {.alphaCutoff = 0.5f,
           .alphaMode = are_flags_set(material.flags, Render_flags::hardedged)
                           ? fx::gltf::Material::AlphaMode::Mask
                           : are_flags_set(material.flags, Render_flags::transparent)
                                ? fx::gltf::Material::AlphaMode::Blend
                                : fx::gltf::Material::AlphaMode::Opaque,
           .doubleSided = are_flags_set(material.flags, Render_flags::doublesided),
           .normalTexture = add_material_normal_texture(doc, material),
           .pbrMetallicRoughness = add_material_pbr(doc, material),
           .name = material.name};
}

auto add_skin_inverse_bind_matrices(fx::gltf::Document& doc,
                                    std::vector<std::uint8_t>& buffer,
                                    [[maybe_unused]] const scene::Scene& scene,
                                    const std::vector<std::uint8_t>& unified_bone_map)
   -> std::int32_t
{
   const auto accessor_index = doc.accessors.size();

   std::vector martices{unified_bone_map.size(), glm::identity<glm::mat4>()};

   doc.accessors.push_back(
      {.bufferView = add_to_buffer(doc, buffer, gsl::make_span(std::as_const(martices))),
       .count = static_cast<std::uint32_t>(martices.size()),
       .componentType = fx::gltf::Accessor::ComponentType::Float,
       .type = fx::gltf::Accessor::Type::Mat4});

   return static_cast<std::uint32_t>(accessor_index);
}

}

void save_scene(scene::Scene scene, File_saver& file_saver)
{
   const auto unified_bone_map = scene::unify_bone_maps(scene);
   unstripfy_scene_nodes_topologies(scene.nodes);

   fx::gltf::Document doc{};

   doc.buffers.emplace_back(); // embedded buffer

   std::vector<std::uint8_t> buffer;
   buffer.reserve(1000000);

   for (const auto& node : scene.nodes) {
      doc.nodes.push_back(
         {.name = node.name,
          .mesh = add_node_mesh(doc, buffer, node),
          .skin = (GLTF_EXPORT_SKIN && scene::has_skinned_geometry(node)) ? 0 : -1,
          .matrix = make_node_matrix(node.transform),
          .children = make_node_children_list(node.name, scene.nodes)});
   }

   for (const auto& material : scene.materials) {
      doc.materials.push_back(add_material(doc, material));
   }

   if constexpr (GLTF_EXPORT_SKIN && scene::has_skinned_geometry(scene)) {
      doc.skins.push_back(
         {.inverseBindMatrices =
             add_skin_inverse_bind_matrices(doc, buffer, scene, unified_bone_map),
          .skeleton = 0,
          .joints = {unified_bone_map.cbegin(), unified_bone_map.cend()}});
   }

   doc.scene = 0;
   doc.scenes.push_back({.name = scene.name, .nodes = {0}});
   doc.buffers.front() = {.byteLength = gsl::narrow<std::uint32_t>(buffer.size()),
                          .data = std::move(buffer)};

   fx::gltf::Save(doc, file_saver.build_file_path(""sv, scene.name, ".glb"sv).string(),
                  true);
}

}
