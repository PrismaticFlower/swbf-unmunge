
#include "magic_number.hpp"
#include "model_builder.hpp"
#include "ucfb_reader.hpp"

#include <ctre.hpp>

namespace {

constexpr static auto name_match_pattern = ctll::fixed_string{R"((p_|P_)?(-.*-_?)?(.*))"};

struct Primitive_Data {
   model::Collision_primitive_type type;
   glm::vec3 size;
};

static_assert(std::is_trivially_copyable_v<Primitive_Data>);
static_assert(sizeof(Primitive_Data) == 16);

auto get_primitive_name(const std::string_view full_name) -> std::string_view
{
   const auto [full, primitve, flags, name] = ctre::match<name_match_pattern>(full_name);

   return name;
}

auto read_next_primitive(Ucfb_reader& primitives) -> model::Collision_primitive
{
   model::Collision_primitive prim;

   prim.name =
      get_primitive_name(primitives.read_child_strict<"NAME"_mn>().read_string());

   auto mask = primitives.read_child_strict_optional<"MASK"_mn>();

   if (mask) {
      prim.flags =
         static_cast<model::Collision_flags>(mask->read_trivial<std::uint8_t>());
   }

   prim.parent = primitives.read_child_strict<"PRNT"_mn>().read_string();
   prim.transform = primitives.read_child_strict<"XFRM"_mn>().read_trivial<glm::mat4x3>();

   const auto data =
      primitives.read_child_strict<"DATA"_mn>().read_trivial<Primitive_Data>();

   prim.type = data.type;
   prim.size = data.size;

   return prim;
}
}

void handle_primitives(Ucfb_reader primitives, model::Models_builder& builders)
{
   auto info = primitives.read_child_strict<"INFO"_mn>();

   const auto name{info.read_string_unaligned()};
   const auto primitive_count = info.read_trivial<std::int32_t>();

   model::Model model{.name = std::string{name}};
   model.collision_primitives.reserve(primitive_count);

   for (auto i = 0; i < primitive_count; ++i) {
      model.collision_primitives.emplace_back(read_next_primitive(primitives));
   }

   builders.integrate(std::move(model));
}