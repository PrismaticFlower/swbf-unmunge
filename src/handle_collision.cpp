
#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

namespace {

struct Collision_info {
   std::uint32_t vertex_count;
   std::uint32_t node_count;
   std::uint32_t leaf_count;
   std::uint32_t unknown;

   float bbox_info[6];
};

static_assert(std::is_standard_layout_v<Collision_info>);
static_assert(sizeof(Collision_info) == 40);

std::vector<glm::vec3> read_positions(Ucfb_reader_strict<"POSI"_mn> vertices,
                                      const std::size_t vertex_count)
{
   std::vector<glm::vec3> buffer;
   buffer.resize(vertex_count);

   const auto vertex_array = vertices.read_array<pod::Vec3>(vertex_count);

   std::memcpy(buffer.data(), vertex_array.data(), buffer.size() * sizeof(glm::vec3));

   return buffer;
}

std::vector<std::uint16_t> read_tree_leaf(Ucfb_reader_strict<"LEAF"_mn> leaf)
{
   std::uint8_t index_count = leaf.read_trivial_unaligned<std::uint8_t>();
   leaf.consume_unaligned(6);

   const auto indices = leaf.read_array<std::uint16_t>(index_count);

   return {std::cbegin(indices), std::cend(indices)};
}

void handle_tree(Ucfb_reader_strict<"TREE"_mn> tree, msh::Collsion_mesh& collision_mesh)
{
   while (tree) {
      const auto child = tree.read_child();

      if (child.magic_number() == "LEAF"_mn) {
         collision_mesh.strips.emplace_back(
            read_tree_leaf(Ucfb_reader_strict<"LEAF"_mn>{child}));
      }
   }
}
}

void handle_collision(Ucfb_reader collision, msh::Builders_map& builders)
{
   const std::string name{collision.read_child_strict<"NAME"_mn>().read_string()};

   auto mask = collision.read_child_strict_optional<"MASK"_mn>();

   msh::Collision_flags flags = msh::Collision_flags::all;

   if (mask) {
      flags = static_cast<msh::Collision_flags>(mask->read_trivial<std::uint8_t>());
   }

   const auto parent = collision.read_child_strict<"NODE"_mn>().read_string();

   const auto info =
      collision.read_child_strict<"INFO"_mn>().read_trivial<Collision_info>();

   msh::Collsion_mesh collision_mesh;
   collision_mesh.parent = parent;
   collision_mesh.strips.reserve(info.leaf_count);
   collision_mesh.flags = flags;

   collision_mesh.vertices =
      read_positions(collision.read_child_strict<"POSI"_mn>(), info.vertex_count);

   handle_tree(collision.read_child_strict<"TREE"_mn>(), collision_mesh);

   builders[name].add_collision_mesh(std::move(collision_mesh));
}
