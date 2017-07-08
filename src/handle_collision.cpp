
#include "chunk_headers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "type_pun.hpp"

namespace {

struct Collision_info {
   Magic_number mn;
   std::uint32_t size;

   std::uint32_t vertex_count;
   std::uint32_t node_count;
   std::uint32_t leaf_count;
   std::uint32_t unknown;

   float bbox_info[6];
};

static_assert(std::is_standard_layout_v<Collision_info>);
static_assert(sizeof(Collision_info) == 48);

struct Parent_node {
   Magic_number mn;
   std::uint32_t size;
   char str[];
};

static_assert(std::is_standard_layout_v<Parent_node>);
static_assert(sizeof(Parent_node) == 8);

struct Vertices {
   Magic_number mn;
   std::uint32_t size;
   glm::vec3 positions[];
};

static_assert(std::is_standard_layout_v<Vertices>);
static_assert(sizeof(Vertices) == 8);

struct Tree {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Tree>);
static_assert(sizeof(Tree) == 8);

#pragma pack(push, 1)

struct Tree_leaf {
   Magic_number mn;
   std::uint32_t size;

   std::uint8_t index_count;
   std::uint8_t unknown_0;
   std::uint8_t unknown_1;
   std::uint8_t unknown_2;
   std::uint8_t unknown_3;
   std::uint8_t unknown_4;
   std::uint8_t unknown_5;

   std::uint16_t indices[];
};

static_assert(std::is_standard_layout_v<Tree_leaf>);
static_assert(sizeof(Tree_leaf) == 15);

#pragma pack(pop)

std::string read_parent(const Parent_node& node)
{
   return {&node.str[0], node.size - 1};
}

std::vector<glm::vec3> read_positions(const Vertices& vertices)
{
   std::vector<glm::vec3> buffer;
   buffer.resize(vertices.size / sizeof(glm::vec3));

   std::memcpy(buffer.data(), &vertices.positions[0], buffer.size() * sizeof(glm::vec3));

   return buffer;
}

std::vector<std::uint16_t> read_tree_leaf(const Tree_leaf& leaf)
{
   std::vector<std::uint16_t> strip;
   strip.reserve(leaf.index_count);

   for (std::size_t i = 0; i < leaf.index_count; ++i) {
      strip.push_back(leaf.indices[i]);
   }

   return strip;
}

void handle_tree(const Tree& tree, msh::Collsion_mesh& collision_mesh)
{
   std::uint32_t head = 0;
   const std::uint32_t end = tree.size - 8;

   while (head < end) {
      const auto& child = view_type_as<chunks::Unknown>(tree.bytes[head]);

      if (child.mn == "LEAF"_mn) {
         const auto& leaf = view_type_as<Tree_leaf>(child);

         collision_mesh.strips.emplace_back(read_tree_leaf(leaf));
      }

      head += child.size + 8;
      if (head % 4 != 0) head += (4 - (head % 4));
   }
}
}

void handle_collision(const chunks::Collision& coll, msh::Builders_map& builders)
{
   std::string name{reinterpret_cast<const char*>(&coll.bytes[0]), coll.name_size - 1};

   std::uint32_t head = coll.name_size;
   const std::uint32_t end = coll.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };
   align_head();

   msh::Collsion_mesh collision_mesh;

   while (head < end) {
      const auto& child = view_type_as<chunks::Unknown>(coll.bytes[head]);

      if (child.mn == "INFO"_mn) {
         const auto& info = view_type_as<Collision_info>(child);

         collision_mesh.strips.reserve(info.leaf_count);
      }
      else if (child.mn == "NODE"_mn) {
         collision_mesh.parent = read_parent(view_type_as<Parent_node>(child));
      }
      else if (child.mn == "POSI"_mn) {
         collision_mesh.vertices = read_positions(view_type_as<Vertices>(child));
      }
      else if (child.mn == "TREE"_mn) {
         handle_tree(view_type_as<Tree>(child), collision_mesh);
      }

      head += child.size + 8;
      align_head();
   }

   builders[name].add_collision_mesh(std::move(collision_mesh));
}
