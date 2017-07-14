#pragma once

#include "file_saver.hpp"

#define GLM_FORCE_CXX98
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_vector.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace msh {

enum class Render_flags : std::uint8_t {
   normal = 0,
   emissive = 1,
   glow = 2,
   singlesided = 4,
   doublesided = 8,
   hardedged = 16,
   perpixel = 32,
   additive = 64,
   specular = 128,
};

enum class Render_type : std::uint8_t {
   normal = 0,
   scrolling = 3,
   env_map = 6,
   animated = 7,
   glow = 11,
   wireframe = 24,
   energy = 25,
   refraction = 22,
   bumpmap = 27
};

enum class Primitive_type : std::uint32_t { sphere = 0, cylinder = 2, cube = 4 };

enum class Collision_flags : std::uint32_t {
   soldier = 1,
   vehicle = 2,
   building = 4,
   terrain = 8,
   ordnance = 16,
   flyer = 32,

   all = 63
};

struct Material {
   glm::vec4 colour;
   float specular_value;
   std::array<std::string, 4> textures;

   Render_flags flags;
   Render_type type;
   std::array<std::uint8_t, 2> params;
};

struct Model {
   std::string parent;

   Material material;

   std::vector<std::vector<std::uint16_t>> strips;
   std::vector<glm::vec3> vertices;
   std::vector<glm::vec3> normals;
   std::vector<std::array<std::uint8_t, 4>> colours;
   std::vector<glm::vec2> texture_coords;
   std::vector<std::uint8_t> skin;
   std::vector<std::uint8_t> bone_map;
};

struct Shadow {
   std::string parent;

   std::vector<std::vector<std::uint16_t>> strips;
   std::vector<glm::vec3> vertices;
   std::vector<std::uint8_t> skin;
   std::vector<std::uint8_t> bone_map;
};

struct Bone {
   std::string name;
   std::string parent;
   glm::vec3 position;
   glm::quat rotation;
};

struct Collsion_mesh {
   std::string parent;

   std::vector<glm::vec3> vertices;
   std::vector<std::vector<std::uint16_t>> strips;
};

struct Collision_primitive {
   std::string parent;

   Primitive_type type = Primitive_type::cube;
   Collision_flags flags = Collision_flags::all;

   glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
   glm::vec3 position{0.0f, 0.0f, 0.0f};
   glm::vec3 size{0.0f, 0.0f, 0.0f};
};

class Builder {
public:
   void add_bone(Bone bone);

   void add_model(Model model);

   void add_shadow(Shadow shadow);

   void add_collision_mesh(Collsion_mesh collision_mesh);

   void add_collision_primitive(Collision_primitive primitive);

   void set_bbox_extent(glm::vec3 extent);

   void save(const std::string& name, File_saver& file_saver) const;

private:
   tbb::concurrent_vector<Bone> _bones;
   tbb::concurrent_vector<Model> _models;
   tbb::concurrent_vector<Shadow> _shadows;
   tbb::concurrent_vector<Collsion_mesh> _collision_meshes;
   tbb::concurrent_vector<Collision_primitive> _collision_primitives;

   glm::vec3 _bbox_extent;
};

using Builders_map = tbb::concurrent_unordered_map<std::string, Builder>;

void save_all(File_saver& file_saver, const Builders_map& builders);
}