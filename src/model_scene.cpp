
#include "model_scene.hpp"

#include <limits>

namespace model::scene {

namespace {

template<typename Type>
void vertices_aabb(const Type& vertices, AABB& global_aabb,
                   const glm::mat3x4 local_to_global, AABB& local_aabb) noexcept
{
   std::for_each_n(vertices.positions.get(), vertices.size, [&](const glm::vec3 pos) {
      const auto global_pos = glm::vec4{pos, 1.0f} * local_to_global;

      global_aabb.min = glm::min(global_aabb.min, global_pos);
      global_aabb.max = glm::max(global_aabb.max, global_pos);

      local_aabb.min = glm::min(local_aabb.min, pos);
      local_aabb.max = glm::max(local_aabb.max, pos);
   });
}

auto build_node_matrix(const std::vector<Node>& nodes, const Node& child) noexcept
   -> glm::mat3x4
{
   glm::mat4 matrix = child.transform;
   std::string_view next_parent = child.parent;

   const auto is_next_parent = [&next_parent](const Node& node) {
      return node.name == next_parent;
   };

   for (auto it = std::find_if(nodes.cbegin(), nodes.cend(), is_next_parent);
        it != nodes.cend();
        it = std::find_if(nodes.cbegin(), nodes.cend(), is_next_parent)) {
      next_parent = it->parent;
      matrix *= glm::mat4{it->transform};

      if (next_parent.empty()) break;
   }

   return glm::mat3x4{matrix};
}

}

void reverse_pretransforms(Scene& scene) noexcept
{
   for (auto& node : scene.nodes) {
      if (!node.geometry || !node.geometry->vertices.pretransformed) continue;

      const auto matrix = build_node_matrix(scene.nodes, node);
      const auto inv_matrix = glm::mat3x4{glm::inverse(glm::mat4{matrix})};
      const auto inv_rot_matrix = glm::inverse(glm::mat3{matrix});

      std::for_each_n(node.geometry->vertices.positions.get(),
                      node.geometry->vertices.size, [&](glm::vec3& p) {
                         p = glm::vec4{p, 1.0f} * inv_matrix;
                      });

      std::for_each_n(node.geometry->vertices.normals.get(), node.geometry->vertices.size,
                      [&](glm::vec3& n) { n = n * inv_rot_matrix; });

      std::for_each_n(node.geometry->vertices.tangents.get(),
                      node.geometry->vertices.size,
                      [&](glm::vec3& t) { t = t * inv_rot_matrix; });

      std::for_each_n(node.geometry->vertices.bitangents.get(),
                      node.geometry->vertices.size,
                      [&](glm::vec3& b) { b = b * inv_rot_matrix; });

      node.geometry->vertices.pretransformed = false;
   }
}

void recreate_aabbs(Scene& scene) noexcept
{
   scene.aabb = {.min = glm::vec3{std::numeric_limits<float>::max()},
                 .max = glm::vec3{std::numeric_limits<float>::min()}};

   for (auto& node : scene.nodes) {
      if (!node.geometry || !node.cloth_geometry) continue;

      node.aabb = {.min = glm::vec3{std::numeric_limits<float>::max()},
                   .max = glm::vec3{std::numeric_limits<float>::min()}};

      if (node.geometry) {
         vertices_aabb(node.geometry->vertices, scene.aabb,
                       build_node_matrix(scene.nodes, node), node.aabb);
      }
      if (node.cloth_geometry) {
         vertices_aabb(node.geometry->vertices, scene.aabb,
                       build_node_matrix(scene.nodes, node), node.aabb);
      }
   }
}

}