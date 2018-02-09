
#include "msh_builder.hpp"
#include "type_pun.hpp"

#include "glm/glm.hpp"

#include <gsl/gsl>

#include <algorithm>
#include <functional>
#include <tuple>
#include <unordered_map>

namespace {

using namespace msh;

struct Uvec2_hasher {
   auto operator()(const glm::uvec2 value) const noexcept
   {
      static_assert(sizeof(glm::uvec2) == sizeof(std::uint64_t));

      return std::hash<std::uint64_t>{}(view_type_as<std::uint64_t>(value));
   }
};

auto create_face_map(const std::vector<glm::uvec3>& faces)
   -> std::unordered_multimap<glm::uvec2, glm::uvec3, Uvec2_hasher>
{
   std::unordered_multimap<glm::uvec2, glm::uvec3, Uvec2_hasher> face_map;

   for (const auto& face : faces) {
      face_map.emplace(face.xy(), face);
   }

   return face_map;
}

const Bone& find_nearest_bone(const glm::vec3 vertex, const std::vector<Bone>& bones)
{
   Expects(!bones.empty());

   const auto compare = [vertex](const Bone& left, const Bone& right) {
      return (glm::distance(left.position, vertex) <
              glm::distance(right.position, vertex));
   };

   const auto nearest = std::min_element(std::cbegin(bones), std::cend(bones), compare);

   return *nearest;
}

Material create_cloth_material(std::string_view texture_name)
{
   Material material{};
   material.textures[0] = texture_name;
   material.flags = Render_flags::doublesided;
   material.type = Render_type::normal;

   return material;
}

auto create_cloth_normals(const std::vector<glm::vec3>& positions,
                          const std::vector<glm::uvec3>& faces) -> std::vector<glm::vec3>
{
   if (positions.empty()) return {};

   std::vector<glm::vec3> normals;
   normals.resize(positions.size());

   const auto max_vertex = static_cast<std::uint32_t>(positions.size()) - 1;

   for (const auto& face : faces) {
      if (std::max({face.x, face.y, face.z, max_vertex}) != max_vertex) {
         throw std::out_of_range{"Face contained invalid vertex index"};
      }

      const auto e1 = positions[face.x] - positions[face.y];
      const auto e2 = positions[face.z] - positions[face.y];
      const auto no = glm::cross(e1, e2);

      normals[face.x] += no;
      normals[face.y] += no;
      normals[face.z] += no;
   }

   for (auto& normal : normals) {
      normal = glm::normalize(normal);
   }

   return normals;
}

auto create_cloth_strips(const std::vector<glm::uvec3>& faces)
   -> std::vector<std::vector<std::uint16_t>>
{
   std::vector<std::vector<std::uint16_t>> strips;

   auto face_map = create_face_map(faces);

   while (!face_map.empty()) {
      const auto begin = std::cbegin(face_map);

      std::vector<std::uint16_t> strip{static_cast<std::uint16_t>(begin->second.x),
                                       static_cast<std::uint16_t>(begin->second.y),
                                       static_cast<std::uint16_t>(begin->second.z)};

      glm::uvec2 last_edge = {begin->second.y, begin->second.z};

      decltype(face_map)::const_iterator connecting{};

      while ((connecting = face_map.find(last_edge)) != std::cend(face_map)) {
         last_edge.x = strip.back();
         last_edge.y = connecting->second.z;

         strip.emplace_back(static_cast<std::uint16_t>(connecting->second.z));

         face_map.erase(connecting);
      }

      strips.emplace_back(std::move(strip));

      face_map.erase(begin);
   }

   return strips;
}

auto create_cloth_skin(const std::vector<glm::vec3>& positions,
                       const std::vector<Bone>& bones)
   -> std::tuple<std::vector<Skin_entry>, std::vector<std::uint8_t>>
{
   std::vector<Skin_entry> skin;
   skin.reserve(positions.size());

   std::unordered_map<std::string_view, std::uint8_t> used_bones;

   const auto add_used_bone = [&used_bones](std::string_view name) -> std::uint8_t {
      auto bone_iter = used_bones.find(name);

      if (bone_iter == std::end(used_bones)) {
         const auto index = static_cast<std::uint8_t>(used_bones.size());

         bone_iter = used_bones.emplace(name, index).first;
      }

      return bone_iter->second;
   };

   for (const auto& vertex : positions) {
      const auto& nearest = find_nearest_bone(vertex, bones);

      skin.emplace_back(Skin_entry{glm::u8vec3{add_used_bone(nearest.name)},
                                   glm::vec3{1.0f, 0.0f, 0.0f}});
   }

   std::vector<std::uint8_t> bone_map;
   bone_map.reserve(used_bones.size());

   for (const auto& used_bone : used_bones) {
      bone_map.emplace_back(used_bone.second);
   }

   return {std::move(skin), std::move(bone_map)};
}
}

namespace msh {

Model cloth_to_model(const Cloth& cloth, const std::vector<Bone>& bones)
{
   Expects(!bones.empty());

   Model model{};
   model.parent = cloth.parent;
   model.material = create_cloth_material(cloth.texture_name);
   model.rotation = cloth.rotation;
   model.position = cloth.position;

   model.positions = cloth.positions;
   model.normals = create_cloth_normals(cloth.positions, cloth.indices);
   model.texture_coords = cloth.texture_coords;
   model.strips = create_cloth_strips(cloth.indices);

   std::tie(model.skin, model.bone_map) = create_cloth_skin(cloth.positions, bones);

   return model;
}
}
