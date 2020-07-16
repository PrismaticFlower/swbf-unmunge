
#include "model_scene.hpp"

#include <limits>

namespace model::scene {

namespace {

struct Node_inv_transform {
   glm::mat3 inv_matrix;
   glm::vec3 offset;
};

template<typename Type>
void vertices_aabb(const Type& vertices, AABB& global_aabb,
                   const glm::mat4x3 local_to_global, AABB& local_aabb) noexcept
{

//TODO: FIX MESSY, ITERABLE PTRs SEEMS TO BE VISUAL C++ FEATURE
#ifndef _WIN32
  
  auto positionsPtr = vertices.positions.get();
  for (int i = 0; i < vertices.size; i++){

    glm::vec3 pos = positionsPtr[i];

    auto global_pos = local_to_global * glm::vec4(pos, 1.0f);

    global_aabb.min = glm::min(global_aabb.min, global_pos);
    global_aabb.max = glm::max(global_aabb.max, global_pos);

    local_aabb.min = glm::min(local_aabb.min, pos);
    local_aabb.max = glm::max(local_aabb.max, pos);
  }
  
#else

  //Visual C++ seems to convert pointers to simple iterators...
  std::for_each_n(vertices.positions.get(), vertices.size, [&](const glm::vec3 pos) {
    const auto global_pos = local_to_global * glm::vec4{pos, 1.0f};

    global_aabb.min = glm::min(global_aabb.min, global_pos);
    global_aabb.max = glm::max(global_aabb.max, global_pos);

    local_aabb.min = glm::min(local_aabb.min, pos);
    local_aabb.max = glm::max(local_aabb.max, pos);
  });

#endif
}

auto build_node_matrix(const std::vector<Node>& nodes, const Node& child) noexcept
   -> glm::mat4x3
{
   glm::mat4 matrix = child.transform;

//TODO: FIX MESSY
#ifndef _WIN32 
   matrix[3] *= -1.0f;
#else
   matrix[3].xyz = matrix[3].xyz * -1.0f;
#endif

   std::string_view next_parent = child.parent;

   const auto is_next_parent = [&next_parent](const Node& node) {
      return node.name == next_parent;
   };

   for (auto it = std::find_if(nodes.cbegin(), nodes.cend(), is_next_parent);
        it != nodes.cend();
        it = std::find_if(nodes.cbegin(), nodes.cend(), is_next_parent)) {
      next_parent = it->parent;
      matrix = glm::mat4{it->transform} * matrix;

      if (next_parent.empty()) break;
   }

   return glm::mat4x3{matrix};
}

auto build_nodes_inv_transforms(const std::vector<Node>& nodes)
   -> std::vector<Node_inv_transform>
{
   std::vector<Node_inv_transform> matrices;
   matrices.reserve(nodes.size());

   for (auto& node : nodes) {
      matrices.push_back({.inv_matrix = glm::inverse(glm::mat3{node.transform}),
                          .offset = node.transform[3]});
   }

   return matrices;
}

}

void reverse_pretransforms(Scene& scene) noexcept
{
   const auto node_inv_transforms = build_nodes_inv_transforms(scene.nodes);

   for (auto& node : scene.nodes) {
      if (!node.geometry || !node.geometry->vertices.pretransformed ||
          node.geometry->bone_map.empty()) {
         continue;
      }

      auto& vertices = node.geometry->vertices;

      if (!vertices.bones) continue;

      for (std::size_t i = 0; i < vertices.size; ++i) {
         const auto apply_transform = [&](const Node_inv_transform transform) {
            if (vertices.positions) {
               vertices.positions[i] =
                  vertices.positions[i] * transform.inv_matrix + transform.offset;
            }
            if (vertices.normals) {
               vertices.normals[i] = vertices.normals[i] * transform.inv_matrix;
            }
            if (vertices.tangents) {
               vertices.tangents[i] = vertices.tangents[i] * transform.inv_matrix;
            }
            if (vertices.bitangents) {
               vertices.bitangents[i] = vertices.bitangents[i] * transform.inv_matrix;
            }
         };

         const auto first_skin_node_index =
            node.geometry->bone_map.at(vertices.bones[i].x);
         const auto& skin_node = scene.nodes.at(first_skin_node_index);

         apply_transform(node_inv_transforms.at(first_skin_node_index));

         for (auto it = std::find_if(
                 scene.nodes.cbegin(), scene.nodes.cend(),
                 [&](const Node& node) { return skin_node.parent == node.name; });
              it != scene.nodes.cend();
              it = std::find_if(
                 scene.nodes.cbegin(), scene.nodes.cend(),
                 [&](const Node& node) { return it->parent == node.name; })) {
            apply_transform(
               node_inv_transforms.at(std::distance(scene.nodes.cbegin(), it)));
         }
      }

      vertices.pretransformed = false;
   }
}

void recreate_aabbs(Scene& scene) noexcept
{
   scene.aabb = {.min = glm::vec3{std::numeric_limits<float>::max()},
                 .max = glm::vec3{std::numeric_limits<float>::min()}};

   for (auto& node : scene.nodes) {
      if (!node.geometry && !node.cloth_geometry) continue;

      node.aabb = {.min = glm::vec3{std::numeric_limits<float>::max()},
                   .max = glm::vec3{std::numeric_limits<float>::min()}};

      if (node.geometry) {
         vertices_aabb(node.geometry->vertices, scene.aabb,
                       build_node_matrix(scene.nodes, node), node.aabb);
      }
      if (node.cloth_geometry) {
         vertices_aabb(node.cloth_geometry->vertices, scene.aabb,
                       build_node_matrix(scene.nodes, node), node.aabb);
      }
   }
}

bool has_collision_geometry(const Scene& scene) noexcept
{
   for (const auto& node : scene.nodes) {
      if (node.type == Node_type::collision) return true;
   }

   return false;
}

bool has_skinned_geometry(const Node& node) noexcept
{
   return node.geometry && !node.geometry->bone_map.empty() &&
          node.geometry->vertices.bones;
}

bool has_skinned_geometry(const Scene& scene) noexcept
{
   for (const auto& node : scene.nodes) {
      if (has_skinned_geometry(node)) return true;
   }

   return false;
}

auto unify_bone_maps(Scene& scene) -> std::vector<std::uint8_t>
{
   if (scene.nodes.size() > std::numeric_limits<std::uint8_t>::max()) {
      throw std::runtime_error{"Scene has too many nodes to unify bone maps."};
   }

   std::vector<std::uint8_t> bone_map;
   bone_map.reserve(scene.nodes.size() * scene.nodes.size());

   for (auto& node : scene.nodes) {
      if (has_skinned_geometry(node)) {
         bone_map.insert(bone_map.cend(), node.geometry->bone_map.cbegin(),
                         node.geometry->bone_map.cend());
      }
   }

   std::sort(bone_map.begin(), bone_map.end());
   bone_map.erase(std::unique(bone_map.begin(), bone_map.end()), bone_map.end());

   for (auto& node : scene.nodes) {
      if (!node.geometry) continue;

      auto& geom = node.geometry.value();

      if (geom.bone_map.empty()) continue;

      if (!geom.vertices.bones) {
         geom.bone_map.clear();
         continue;
      }

      std::array<std::uint8_t, 255> bones_lut;

      if (geom.bone_map.size() > bones_lut.size()) {
         throw std::runtime_error{"Geometry has invalid bone map."};
      }

      std::transform(
         geom.bone_map.cbegin(), geom.bone_map.cend(), bones_lut.begin(),
         [&](const std::uint8_t i) {
            return static_cast<std::uint8_t>(std::distance(
               bone_map.cbegin(), std::find(bone_map.cbegin(), bone_map.cend(), i)));
         });

      geom.bone_map = bone_map;

      std::transform(geom.vertices.bones.get(),
                     geom.vertices.bones.get() + geom.vertices.size,
                     geom.vertices.bones.get(), [&](const glm::u8vec3 bones) {
                        return glm::u8vec3{bones_lut.at(bones[0]), bones_lut.at(bones[1]),
                                           bones_lut.at(bones[2])};
                     });
   }

   return bone_map;
}

}