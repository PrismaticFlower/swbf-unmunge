#pragma once

#include "app_options.hpp"
#include "file_saver.hpp"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_vector.h"
#include "tbb/spin_mutex.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace msh {

enum class Render_flags : std::uint8_t {
   normal = 0,
   emissive = 1,
   glow = 2,
   transparent = 4,
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

enum class Render_type_swbf1 : std::uint8_t {
   normal = 0,
   glow = 1,
   scroll = 3,
   specular = 4,
   reflection = 6,
   water = 10,
   detail = 11,
   refraction = 22,
   camouflage = 23,
   bumpmap = 27,
   bumpmap_specular = 28
};

enum class Primitive_type : std::uint32_t { sphere = 0, cylinder = 2, cube = 4 };

enum class Collision_flags : std::uint32_t {
   all = 0,
   soldier = 1,
   vehicle = 2,
   building = 4,
   terrain = 8,
   ordnance = 16,
   flyer = 32
};

enum class Cloth_collision_type : std::uint32_t { sphere = 0, cylinder = 1, cube = 2 };

struct Bbox {
   glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
   glm::vec3 centre{0.0f, 0.0f, 0.0f};
   glm::vec3 size{0.0f, 0.0f, 0.0f};
};

struct Material {
   glm::vec4 diffuse_colour{1.0f, 1.0f, 1.0f, 1.0f};
   glm::vec4 specular_colour{1.0f, 1.0f, 1.0f, 1.0f};
   float specular_value{20.0f};
   std::array<std::string, 4> textures;

   Render_flags flags;
   union {
      Render_type type;
      Render_type_swbf1 type_swbf1;

      static_assert(std::is_same_v<std::underlying_type_t<Render_type>,
                                   std::underlying_type_t<Render_type_swbf1>>);
   };
   std::array<std::int8_t, 2> params;
};

struct Skin_entry {
   glm::u8vec3 bones;
   glm::vec3 weights;
};

enum class Lod {
   zero,
   one,
   two,
   lowres,
};

struct Model {
   std::optional<std::string> parent;
   std::optional<std::string> name;

   glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
   glm::vec3 position;

   Material material;

   std::vector<std::vector<std::uint16_t>> strips;
   std::vector<glm::vec3> positions;
   std::vector<glm::vec3> normals;
   std::vector<glm::vec4> colours;
   std::vector<glm::vec2> texture_coords;
   std::vector<Skin_entry> skin;
   std::vector<std::uint8_t> bone_map;
   bool pretransformed = false;
   Lod lod{Lod::zero};
};

struct Bone {
   std::string name;
   std::string parent;
   glm::vec3 position;
   glm::quat rotation;
};

struct Collsion_mesh {
   std::optional<std::string> parent;
   Collision_flags flags = Collision_flags::all;

   std::vector<glm::vec3> positions;
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

struct Cloth_collision;

struct Cloth {
   std::string name;
   std::string parent;

   glm::quat rotation;
   glm::vec3 position;

   std::string texture_name;

   std::vector<glm::vec3> positions;
   std::vector<glm::vec2> texture_coords;
   std::vector<std::uint32_t> fixed_points;
   std::vector<std::string> fixed_weights;
   std::vector<glm::uvec3> indices;
   std::vector<std::array<std::uint16_t, 2>> stretch_constraints;
   std::vector<std::array<std::uint16_t, 2>> cross_constraints;
   std::vector<std::array<std::uint16_t, 2>> bend_constraints;

   std::vector<Cloth_collision> collision;
};

struct Cloth_collision {
   std::string parent;
   Cloth_collision_type type = Cloth_collision_type::sphere;
   glm::vec3 size{0.0f, 0.0f, 0.0f};
};

class Builder {
public:
   Builder() = default;

   Builder(const Builder& other);

   void add_bone(Bone bone);

   void add_model(Model model);
   void add_collision_mesh(Collsion_mesh collision_mesh);

   void add_collision_primitive(Collision_primitive primitive);

   void add_cloth(Cloth cloth);

   void set_bbox(const Bbox& bbox) noexcept;

   void save(const std::string& name, File_saver& file_saver,
             const Game_version version) const;

private:
   Bbox get_bbox() const noexcept;

   tbb::concurrent_vector<Bone> _bones;
   tbb::concurrent_vector<Model> _models;
   tbb::concurrent_vector<Collsion_mesh> _collision_meshes;
   tbb::concurrent_vector<Collision_primitive> _collision_primitives;
   tbb::concurrent_vector<Cloth> _cloths;

   mutable tbb::spin_mutex _bbox_mutex;
   Bbox _bbox;
};

using Builders_map = tbb::concurrent_unordered_map<std::string, Builder>;

void save_all(File_saver& file_saver, const Builders_map& builders,
              const Game_version version);
}
