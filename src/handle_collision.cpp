
#include "magic_number.hpp"
#include "model_builder.hpp"
#include "synced_cout.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

namespace {

struct Collision_info {
   std::uint32_t vertex_count;
   std::uint32_t node_count;
   std::uint32_t leaf_count;
   std::uint32_t
      index_count; // (?) probably not exactly this but seems to hold similar semantics
   std::array<glm::vec3, 2> aabb;
};

static_assert(std::is_standard_layout_v<Collision_info>);
static_assert(sizeof(Collision_info) == 40);

void triangulate_points(const std::vector<std::uint16_t>& points, model::Indices& out)
{
   if (points.size() == 1) {
      synced_cout::print("Found collision geometry represented as a point. Skipping.");

      return;
   }
   else if (points.size() == 2) {
      synced_cout::print("Found collision geometry represented as a line. Skipping.");

      return;
   }

   for (std::size_t i = 2; i < points.size(); ++i) {
      out.insert(out.end(), {points[0], points[i], points[i - 1]});
   }
}

void read_tree_leaf(Ucfb_reader_strict<"LEAF"_mn> leaf, model::Indices& out)
{
   const std::uint8_t point_count = leaf.read_trivial_unaligned<std::uint8_t>();
   leaf.consume_unaligned(6); // consume six unknown byte fields

   const auto points = leaf.read_array_unaligned<std::uint16_t>(point_count);

   triangulate_points(points, out);
}

auto read_tree(Ucfb_reader_strict<"TREE"_mn> tree, const std::size_t reserve_size)
   -> model::Indices
{
   model::Indices result;
   result.reserve(reserve_size * 3);

   while (tree) {
      const auto child = tree.read_child();

      if (child.magic_number() == "LEAF"_mn) {
         read_tree_leaf(Ucfb_reader_strict<"LEAF"_mn>{child}, result);
      }
   }

   return result;
}
}

void handle_collision(Ucfb_reader collision, model::Models_builder& builders)
{
   const auto name = collision.read_child_strict<"NAME"_mn>().read_string();

   auto mask = collision.read_child_strict_optional<"MASK"_mn>();

   model::Collision_flags flags = model::Collision_flags::all;

   if (mask) {
      flags = static_cast<model::Collision_flags>(mask->read_trivial<std::uint8_t>());
   }

   collision.read_child_strict<"NODE"_mn>();

   const auto info =
      collision.read_child_strict<"INFO"_mn>().read_trivial<Collision_info>();
   auto posi = collision.read_child_strict<"POSI"_mn>();
   auto tree = collision.read_child_strict<"TREE"_mn>();

   builders.integrate(
      {.name = std::string{name},
       .collision_meshes = {model::Collsion_mesh{
          .flags = flags,
          .indices = read_tree(tree, info.index_count *
                                        3), // over reserve more memory than needed
                                            // to account for triangulation results
          .positions = posi.read_array<glm::vec3>(info.vertex_count)}}});
}
