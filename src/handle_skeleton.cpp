
#include "magic_number.hpp"
#include "model_builder.hpp"
#include "ucfb_reader.hpp"

#pragma optimize("", off)

namespace {

auto read_bones(const std::size_t count, Ucfb_reader_strict<"NAME"_mn> name_reader,
                Ucfb_reader_strict<"PRNT"_mn> parent_reader,
                Ucfb_reader_strict<"XFRM"_mn> transform_reader)
   -> std::vector<model::Bone>
{
   std::vector<model::Bone> bones;
   bones.reserve(count);

   for (std::size_t i = 0; i < count; ++i) {
      bones.emplace_back(
         model::Bone{.name = std::string{name_reader.read_string_unaligned()},
                     .parent = std::string{parent_reader.read_string_unaligned()},
                     .transform = transform_reader.read_trivial<glm::mat3x4>()});
   }

   return bones;
}
}

void handle_skeleton(Ucfb_reader skeleton, model::Models_builder& builders)
{
   auto info = skeleton.read_child_strict<"INFO"_mn>();

   const auto name{info.read_string_unaligned()};
   const auto bone_count = info.read_trivial_unaligned<std::uint16_t>();

   const auto name_reader = skeleton.read_child_strict<"NAME"_mn>();
   const auto parent_reader = skeleton.read_child_strict<"PRNT"_mn>();
   const auto transform_reader = skeleton.read_child_strict<"XFRM"_mn>();

   builders.integrate(
      {.name = std::string{name},
       .bones = read_bones(bone_count, name_reader, parent_reader, transform_reader)});
}