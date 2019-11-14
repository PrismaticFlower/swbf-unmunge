#pragma once

#include "bit_flags.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace model {

enum class Render_flags : std::uint8_t {
   normal = 0,
   emissive = 1,
   glow = 2,
   transparent = 4,
   doublesided = 8,
   hardedged = 16,
   perpixel = 32,
   additive = 64,
   specular = 128
};

enum class Render_type : std::uint8_t {
   normal = 0,
   scrolling = 3,
   specular = 4,
   env_map = 6,
   reflection = env_map,
   animated = 7,
   water = 10,
   glow = 11,
   detail = glow,
   refraction = 22,
   camouflage = 23,
   tiled_normalmap = 24,
   energy = 25,
   bumpmap = 27,
   bumpmap_specular = 28
};

enum class Primitive_topology : std::int32_t {
   undefined,
   point_list,
   line_list,
   line_loop,
   line_strip,
   triangle_list,
   triangle_strip,
   triangle_strip_ps2, // triangle strips with primitive restart when the high bit is set
                       // on two consecutive indices.
   triangle_fan
};

enum class Collision_primitive_type : std::uint32_t {
   sphere = 0,
   cylinder = 2,
   cube = 4
};

enum class Collision_flags : std::uint32_t {
   all = 0,
   soldier = 1,
   vehicle = 2,
   building = 4,
   terrain = 8,
   ordnance = 16,
   flyer = 32
};

constexpr bool marked_as_enum_flag(Collision_flags)
{
   return true;
}

enum class Cloth_collision_primitive_type : std::uint32_t {
   sphere = 0,
   cylinder = 1,
   cube = 2
};

enum class Lod { zero, one, two, three, lowres };

using Indices = std::vector<std::uint16_t>;

struct Vertices {
   struct Create_flags {
      bool positions = false;
      bool normals = false;
      bool tangents = false;
      bool bitangents = false;
      bool colors = false;
      bool texcoords = false;
      bool bones = false;
      bool weights = false;
   };

   Vertices() = default;

   Vertices(const std::size_t size, const Create_flags flags);

   std::size_t size = 0;
   bool pretransformed = false;
   bool static_lighting = false;
   bool softskinned = false;
   std::unique_ptr<glm::vec3[]> positions;
   std::unique_ptr<glm::vec3[]> normals;
   std::unique_ptr<glm::vec3[]> tangents;
   std::unique_ptr<glm::vec3[]> bitangents;
   std::unique_ptr<glm::vec4[]> colors;
   std::unique_ptr<glm::vec2[]> texcoords;
   std::unique_ptr<glm::u8vec3[]> bones;
   std::unique_ptr<glm::vec3[]> weights;
};

using Cloth_indices = std::vector<std::array<std::uint32_t, 3>>;

struct Cloth_vertices {
   Cloth_vertices() = default;

   Cloth_vertices(const std::size_t size)
      : size{size}, positions{std::make_unique<glm::vec3[]>(size)},
        texcoords{std::make_unique<glm::vec2[]>(size)}
   {
   }

   std::size_t size = 0;
   std::unique_ptr<glm::vec3[]> positions;
   std::unique_ptr<glm::vec2[]> texcoords;
};

struct Cloth_collision_primitive {
   std::string parent;
   Cloth_collision_primitive_type type = Cloth_collision_primitive_type::sphere;
   glm::vec3 size{0.0f, 0.0f, 0.0f};
};

constexpr auto to_string_view(const Primitive_topology primitive_topology) noexcept
   -> std::string_view
{
   using namespace std::literals;

   switch (primitive_topology) {
   case Primitive_topology::point_list:
      return "point_list"sv;
   case Primitive_topology::line_list:
      return "line_list"sv;
   case Primitive_topology::line_loop:
      return "line_loop"sv;
   case Primitive_topology::line_strip:
      return "line_strip"sv;
   case Primitive_topology::triangle_list:
      return "triangle_list"sv;
   case Primitive_topology::triangle_strip:
      return "triangle_strip"sv;
   case Primitive_topology::triangle_strip_ps2:
      return "triangle_strip_ps2"sv;
   case Primitive_topology::triangle_fan:
      return "triangle_fan"sv;
   case Primitive_topology::undefined:
   default:
      return "undefined"sv;
   }
}

}