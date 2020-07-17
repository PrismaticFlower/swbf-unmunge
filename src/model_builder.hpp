#pragma once

#include "app_options.hpp"
#include "model_types.hpp"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class File_saver;

namespace model {

struct Bone {
   std::string name;
   std::string parent;
   glm::mat4x3 transform = glm::identity<glm::mat4x3>();
};

struct Material {
   std::optional<std::string> name;

   glm::vec4 diffuse_colour{1.0f, 1.0f, 1.0f, 1.0f};
   glm::vec4 specular_colour{1.0f, 1.0f, 1.0f, 1.0f};
   float specular_exponent{50.0f};
   std::array<std::string, 4> textures;

   Render_flags flags;
   Render_type type;

   std::array<std::int8_t, 2> params;

   std::optional<std::string> attached_light;
   bool vertex_lighting = false;
};

struct Part {
   std::optional<std::string> name;
   std::string parent;
   Lod lod = Lod::zero;

   Material material;

   Primitive_topology primitive_topology = Primitive_topology::undefined;
   Indices indices;
   Vertices vertices;

   std::vector<std::uint8_t> bone_map;

   //Part(Part&& p) = default;
};

struct Collision_primitive {
   std::string name;
   std::string parent;

   Collision_primitive_type type = Collision_primitive_type::cube;
   Collision_flags flags = Collision_flags::all;

   glm::mat4x3 transform = glm::identity<glm::mat4x3>();
   glm::vec3 size{0.0f, 0.0f, 0.0f};
};

struct Collsion_mesh {
   inline constexpr static auto primitive_topology = Primitive_topology::triangle_list;

   Collision_flags flags = Collision_flags::all;

   Indices indices;
   std::vector<glm::vec3> positions;

   //Collsion_mesh(Collsion_mesh&& c) = default;
};

struct Cloth {
   std::string name;
   std::string parent;

   glm::mat4x3 transform = glm::identity<glm::mat4x3>();

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

struct Model {
   std::string name;

   std::vector<Bone> bones;
   std::vector<Part> parts;
   std::vector<Collsion_mesh> collision_meshes;
   std::vector<Collision_primitive> collision_primitives;
   std::vector<Cloth> cloths;

   //Model(Model&& m) = default;

   void merge_with(Model other) noexcept;
};

class Models_builder {
public:
   void integrate(Model model) noexcept;

   void save_models(File_saver& file_saver, const Game_version game_version,
                    const Model_format format,
                    const Model_discard_flags discard_flags) noexcept;

private:
   std::mutex _mutex;
   std::vector<Model> _models;
};

}