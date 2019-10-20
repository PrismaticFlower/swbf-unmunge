
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
   std::for_each_n(vertices.positions.get(), vertices.size, [&](const glm::vec3 pos) {
      const auto global_pos = local_to_global * glm::vec4{pos, 1.0f};

      global_aabb.min = glm::min(global_aabb.min, global_pos);
      global_aabb.max = glm::max(global_aabb.max, global_pos);

      local_aabb.min = glm::min(local_aabb.min, pos);
      local_aabb.max = glm::max(local_aabb.max, pos);
   });
}

auto build_node_matrix(const std::vector<Node>& nodes, const Node& child) noexcept
   -> glm::mat4x3
{
   glm::mat4 matrix = child.transform;
   matrix[3].xyz = matrix[3].xyz * -1.0f;
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

}