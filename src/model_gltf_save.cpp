
#include "model_gltf_save.hpp"
#include "file_saver.hpp"

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
                            const gsl::span<const T> span, const bool add_min_max = false)
   -> std::uint32_t
{
   constexpr auto type = []() {
      if (std::is_same_v<T, glm::vec2>) return fx::gltf::Accessor::Type::Vec2;
      if (std::is_same_v<T, glm::vec3>) return fx::gltf::Accessor::Type::Vec3;
      if (std::is_same_v<T, glm::vec4>) return fx::gltf::Accessor::Type::Vec4;
   }();

   const auto accessor_index = doc.accessors.size();

   doc.accessors.push_back({.bufferView = add_to_buffer(doc, buffer, span),
                            .count = static_cast<std::uint32_t>(span.size()),
                            .componentType = fx::gltf::Accessor::ComponentType::Float,
                            .type = type});

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

   return attribs;
}

auto add_mesh_primitve(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                       const scene::Geometry& geometry) -> fx::gltf::Primitive
{
   return {.indices = add_primitve_indices(doc, buffer, geometry.indices),
           .mode = map_primitive_topology(geometry.topology),
           .attributes = add_primitve_attributes(doc, buffer, geometry.vertices)};
}

auto add_node_mesh(fx::gltf::Document& doc, std::vector<std::uint8_t>& buffer,
                   const scene::Node& node) -> std::int32_t
{
   if (!node.geometry) return -1;

   const auto index = static_cast<std::int32_t>(doc.meshes.size());

   doc.meshes.push_back({.name = node.name,
                         .primitives = {add_mesh_primitve(doc, buffer, *node.geometry)}});

   return index;
}

}

void save_scene(scene::Scene scene, File_saver& file_saver,
                const Game_version game_version)
{
   fx::gltf::Document doc{};
   std::vector<std::uint8_t> buffer;
   buffer.reserve(1000000);

   for (const auto& node : scene.nodes) {
      doc.nodes.push_back({.name = node.name,
                           .mesh = add_node_mesh(doc, buffer, node),
                           .matrix = make_node_matrix(node.transform),
                           .children = make_node_children_list(node.name, scene.nodes)});
   }

   doc.scene = 0;
   doc.scenes.push_back({.name = scene.name, .nodes = {0}});
   doc.buffers.push_back({.byteLength = gsl::narrow<std::uint32_t>(buffer.size()),
                          .data = std::move(buffer)});

   fx::gltf::Save(doc, file_saver.build_file_path(""sv, scene.name, ".glb"sv).string(),
                  true);
}

}
