#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include "glm/gtc/quaternion.hpp"

#include <gsl/gsl>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <utility>

namespace {

struct Collision_info {
   msh::Cloth_collision_type type;
   pod::Vec3 size;
   pod::Mat3 rotation;
   pod::Vec3 position;
};

static_assert(std::is_pod_v<Collision_info>);
static_assert(sizeof(Collision_info) == 64);

glm::vec2 flip_texture_v(const glm::vec2 coords) noexcept
{
   float v = coords.y;

   if (v > 1.0f) {
      v = std::fmod(v, 1.0f);
   }

   return {coords.x, 1.0f - v};
}

auto read_xframe(Ucfb_reader_strict<"XFRM"_mn> xframe) -> std::pair<glm::quat, glm::vec3>
{
   const glm::mat3 matrix = xframe.read_trivial<pod::Mat3>();
   const glm::vec3 position = xframe.read_trivial<pod::Vec3>();

   return {matrix, position};
}

auto read_vertices(gsl::span<const pod::Vec3> vertices) -> std::vector<glm::vec3>
{
   return {vertices.begin(), vertices.end()};
}

auto read_tex_coords(gsl::span<const pod::Vec2> texture_coords) -> std::vector<glm::vec2>
{
   std::vector<glm::vec2> coords;
   coords.reserve(texture_coords.size());

   for (const auto& uv : texture_coords) {
      coords.emplace_back(flip_texture_v(uv));
   }

   return coords;
}

auto generate_fixed_points(std::uint32_t count) -> std::vector<std::uint32_t>
{
   std::vector<std::uint32_t> points;
   points.resize(count);

   std::iota(std::begin(points), std::end(points), 0);

   return points;
}

auto read_fixed_weights(Ucfb_reader_strict<"DATA"_mn>& data, std::uint32_t count)
   -> std::vector<std::string>
{
   std::vector<std::string> weights;
   weights.reserve(count);

   for (std::uint32_t i = 0; i < count; ++i) {
      weights.emplace_back(data.read_string_unaligned());
   }

   return weights;
}

auto read_msh_indices(gsl::span<const std::array<std::uint32_t, 3>> indices)
   -> std::vector<glm::uvec3>
{
   std::vector<glm::uvec3> buffer;
   buffer.reserve(indices.size());

   for (const auto& index : indices) {
      buffer.emplace_back(index[0], index[1], index[2]);
   }

   return buffer;
}

auto read_constraint(gsl::span<const std::array<std::uint32_t, 2>> constraints)
   -> std::vector<std::array<std::uint16_t, 2>>
{
   std::vector<std::array<std::uint16_t, 2>> result;
   result.reserve(constraints.size());

   for (const auto& constraint : constraints) {
      result.emplace_back();
      result.back()[0] = static_cast<std::uint16_t>(constraint[0]);
      result.back()[1] = static_cast<std::uint16_t>(constraint[1]);
   }

   return result;
}

void read_cloth_data(Ucfb_reader_strict<"DATA"_mn> data, msh::Cloth& cloth)
{
   cloth.texture_name = data.read_string_unaligned();

   const auto vertex_count = data.read_trivial_unaligned<std::uint32_t>();

   cloth.positions = read_vertices(data.read_array_unaligned<pod::Vec3>(vertex_count));
   cloth.texture_coords =
      read_tex_coords(data.read_array_unaligned<pod::Vec2>(vertex_count));

   const auto fixed_point_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.fixed_points = generate_fixed_points(fixed_point_count);

   const auto fixed_weight_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.fixed_weights = read_fixed_weights(data, fixed_weight_count);

   const auto index_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.indices = read_msh_indices(
      data.read_array_unaligned<std::array<std::uint32_t, 3>>(index_count));

   using Constraint = std::array<std::uint32_t, 2>;

   const auto stretch_constraint_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.stretch_constraints =
      read_constraint(data.read_array_unaligned<Constraint>(stretch_constraint_count));

   const auto bend_constraint_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.bend_constraints =
      read_constraint(data.read_array_unaligned<Constraint>(bend_constraint_count));

   const auto cross_constraint_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.cross_constraints =
      read_constraint(data.read_array_unaligned<Constraint>(cross_constraint_count));
}

auto read_cloth_collision(Ucfb_reader_strict<"COLL"_mn> collision)
   -> std::vector<msh::Cloth_collision>
{
   const auto count = collision.read_trivial<std::uint32_t>();

   std::vector<msh::Cloth_collision> nodes;
   nodes.reserve(count);

   for (std::uint32_t i = 0; i < count; ++i) {
      msh::Cloth_collision node{};

      node.parent = collision.read_string_unaligned();

      const auto info = collision.read_trivial_unaligned<Collision_info>();

      node.type = info.type;
      node.size = info.size;

      nodes.emplace_back(std::move(node));
   }

   return nodes;
}
}

void handle_cloth(Ucfb_reader cloth, msh::Builders_map& builders)
{
   const std::string model_name{cloth.read_child_strict<"INFO"_mn>().read_string()};

   msh::Cloth cloth_msh{};

   cloth_msh.name = cloth.read_child_strict<"NAME"_mn>().read_string();
   cloth_msh.parent = cloth.read_child_strict<"PRNT"_mn>().read_string();

   const auto xframe = cloth.read_child_strict<"XFRM"_mn>();
   std::tie(cloth_msh.rotation, cloth_msh.position) = read_xframe(xframe);

   read_cloth_data(cloth.read_child_strict<"DATA"_mn>(), cloth_msh);

   cloth_msh.collision = read_cloth_collision(cloth.read_child_strict<"COLL"_mn>());

   auto& builder = builders[model_name];
   builder.add_cloth(std::move(cloth_msh));
}
