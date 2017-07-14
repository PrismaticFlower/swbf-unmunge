
#include "glm_pod_wrappers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include "glm/gtc/quaternion.hpp"

namespace {

struct Xframe {
   pod::Mat3 rotation;
   pod::Vec3 position;
};

static_assert(std::is_pod_v<Xframe>);
static_assert(sizeof(Xframe) == 48);

struct Primitive_Data {
   msh::Primitive_type type;
   pod::Vec3 size;
};

static_assert(std::is_pod_v<Primitive_Data>);
static_assert(sizeof(Primitive_Data) == 16);

auto read_next_primitive(Ucfb_reader& primitives) -> msh::Collision_primitive
{
   msh::Collision_primitive msh_prim;

   primitives.read_child_strict<"NAME"_mn>();

   auto mask = primitives.read_child_strict_optional<"MASK"_mn>();

   if (mask) msh_prim.flags = mask->read_trivial<msh::Collision_flags>();

   msh_prim.parent = primitives.read_child_strict<"PRNT"_mn>().read_string();

   const auto xframe = primitives.read_child_strict<"XFRM"_mn>().read_trivial<Xframe>();

   msh_prim.rotation = glm::quat_cast(glm::mat3{xframe.rotation});
   msh_prim.position = xframe.position;

   const auto data =
      primitives.read_child_strict<"DATA"_mn>().read_trivial<Primitive_Data>();

   msh_prim.type = data.type;
   msh_prim.size = data.size;

   return msh_prim;
}
}

void handle_primitives(Ucfb_reader primitives, msh::Builders_map& builders)
{
   auto info = primitives.read_child_strict<"INFO"_mn>();

   std::string name{info.read_string_unaligned()};
   const auto primitive_count = info.read_trivial<std::int32_t>();

   auto& builder = builders[name];

   for (auto i = 0; i < primitive_count; ++i) {
      builder.add_collision_primitive(read_next_primitive(primitives));
   }
}