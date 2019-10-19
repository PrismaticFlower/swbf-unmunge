#include "magic_number.hpp"
#include "model_builder.hpp"
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
   model::Cloth_collision_primitive_type type;
   glm::vec3 size;
   glm::mat3 rotation;
   glm::vec3 position;
};

static_assert(std::is_trivially_copyable_v<Collision_info>);
static_assert(sizeof(Collision_info) == 64);

glm::vec2 flip_texture_v(const glm::vec2 coords) noexcept
{
   float v = coords.y;

   if (v > 1.0f) {
      v = std::fmod(v, 1.0f);
   }

   return {coords.x, 1.0f - v};
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

void read_cloth_data(Ucfb_reader_strict<"DATA"_mn> data, model::Cloth& cloth)
{
   cloth.texture_name = data.read_string_unaligned();

   const auto vertex_count = data.read_trivial_unaligned<std::uint32_t>();

   cloth.vertices = model::Cloth_vertices{vertex_count};

   data.read_array_to_span_unaligned<glm::vec3>(
      vertex_count, gsl::make_span(cloth.vertices.positions.get(), vertex_count));

   const auto texcoords_span =
      gsl::make_span(cloth.vertices.texcoords.get(), vertex_count);

   data.read_array_to_span_unaligned<glm::vec2>(vertex_count, texcoords_span);
   std::transform(texcoords_span.begin(), texcoords_span.end(), texcoords_span.begin(),
                  flip_texture_v);

   const auto fixed_point_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.fixed_points = generate_fixed_points(fixed_point_count);

   const auto fixed_weight_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.fixed_weights = read_fixed_weights(data, fixed_weight_count);

   const auto index_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.indices = data.read_array_unaligned<std::array<std::uint32_t, 3>>(index_count);

   using Constraint = std::array<std::uint32_t, 2>;

   const auto stretch_constraint_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.stretch_constraints =
      data.read_array_unaligned<Constraint>(stretch_constraint_count);

   const auto bend_constraint_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.bend_constraints = data.read_array_unaligned<Constraint>(bend_constraint_count);

   const auto cross_constraint_count = data.read_trivial_unaligned<std::uint32_t>();
   cloth.cross_constraints =
      data.read_array_unaligned<Constraint>(cross_constraint_count);
}

auto read_cloth_collision(Ucfb_reader_strict<"COLL"_mn> collision)
   -> std::vector<model::Cloth_collision_primitive>
{
   const auto count = collision.read_trivial<std::uint32_t>();

   std::vector<model::Cloth_collision_primitive> nodes;
   nodes.reserve(count);

   for (std::uint32_t i = 0; i < count; ++i) {
      auto& node = nodes.emplace_back();

      node.parent = collision.read_string_unaligned();

      const auto info = collision.read_trivial_unaligned<Collision_info>();

      node.type = info.type;
      node.size = info.size;
   }

   return nodes;
}
}

void handle_cloth(Ucfb_reader cloth, model::Models_builder& models_builder)
{
   const auto model_name = cloth.read_child_strict<"INFO"_mn>().read_string();

   model::Model model{.name = std::string{model_name}};
   auto& cloth_model = model.cloths.emplace_back();

   cloth_model.name = cloth.read_child_strict<"NAME"_mn>().read_string();
   cloth_model.parent = cloth.read_child_strict<"PRNT"_mn>().read_string();
   cloth_model.transform =
      cloth.read_child_strict<"XFRM"_mn>().read_trivial<glm::mat3x4>();

   read_cloth_data(cloth.read_child_strict<"DATA"_mn>(), cloth_model);

   cloth_model.collision = read_cloth_collision(cloth.read_child_strict<"COLL"_mn>());

   models_builder.integrate(std::move(model));
}
