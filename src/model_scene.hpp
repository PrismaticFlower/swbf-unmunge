#pragma once

#include "model_types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace model::scene {

enum class Node_type { null, geometry, cloth_geometry, collision, collision_primitive };

struct Material {
   std::string name;

   glm::vec4 diffuse_colour{1.0f, 1.0f, 1.0f, 1.0f};
   glm::vec4 specular_colour{1.0f, 1.0f, 1.0f, 1.0f};
   float specular_exponent{50.0f};

   Render_flags flags = Render_flags::normal;
   Render_type rendertype = Render_type::normal;

   std::array<std::int8_t, 2> params{};

   std::array<std::string, 4> textures{};

   bool reference_in_option_file = false;

//FIX MESSY: Still don't understand default, and I just can't get
//it to compile.
#ifdef _WIN32
   bool operator==(const Material&) const = default;
#else
   auto operator==(const Material& mat) const {
      return mat.name == name; 
   }
#endif

};

struct Geometry {
   Primitive_topology topology = Primitive_topology::undefined;
   Indices indices;
   Vertices vertices;
   std::vector<std::uint8_t> bone_map;
};

struct Cloth_geometry {
   std::string texture_name;

   Cloth_vertices vertices;
   Cloth_indices indices;
   std::vector<std::uint32_t> fixed_points;
   std::vector<std::string> fixed_weights;
   std::vector<std::array<std::uint32_t, 2>> stretch_constraints;
   std::vector<std::array<std::uint32_t, 2>> cross_constraints;
   std::vector<std::array<std::uint32_t, 2>> bend_constraints;
   std::vector<Cloth_collision_primitive> collision;
};

struct Collision {
   Collision_primitive_type type = Collision_primitive_type::cube;
   glm::vec3 size{0.0f, 0.0f, 0.0f};
};

struct AABB {
   glm::vec3 min;
   glm::vec3 max;
};

struct Node {
   std::string name;
   std::string parent;
   AABB aabb{};

   std::size_t material_index{};
   Node_type type = Node_type::null;
   Lod lod = Lod::zero;

   glm::mat4x3 transform = glm::identity<glm::mat4x3>();

   std::optional<Geometry> geometry;
   std::optional<Cloth_geometry> cloth_geometry;
   std::optional<Collision> collision;
};

struct Attached_light {
   std::string node;
   std::string light;
};

struct Scene {
   std::string name;
   AABB aabb;
   std::vector<Material> materials;
   std::vector<Node> nodes;
   std::vector<Attached_light> attached_lights;

   bool softskin = false;
   bool vertex_lighting = false;
};

void reverse_pretransforms(Scene& scene) noexcept;

void recreate_aabbs(Scene& scene) noexcept;

bool has_collision_geometry(const Scene& scene) noexcept;

bool has_skinned_geometry(const Node& node) noexcept;

bool has_skinned_geometry(const Scene& scene) noexcept;

auto unify_bone_maps(Scene& scene) -> std::vector<std::uint8_t>;
}