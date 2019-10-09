
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include "glm/gtc/quaternion.hpp"

namespace {

struct Xframe {
   glm::mat3 matrix;
   glm::vec3 position;
};

static_assert(std::is_pod_v<Xframe>);
static_assert(sizeof(Xframe) == 48);

auto read_xframe(const Xframe& xframe) -> std::pair<glm::vec3, glm::quat>
{
   return {xframe.position, glm::mat3{xframe.matrix}};
}

template<Magic_number magic_number>
auto read_bone_names(Ucfb_reader_strict<magic_number> names, std::size_t count)
   -> std::vector<std::string_view>
{
   std::vector<std::string_view> strings;
   strings.reserve(count);

   for (std::size_t i{0}; i < count; ++i) {
      strings.emplace_back(names.read_string_unaligned());
   }

   return strings;
}

auto read_bone_xframes(Ucfb_reader_strict<"XFRM"_mn> xframes, std::size_t count)
   -> std::vector<std::pair<glm::vec3, glm::quat>>
{
   std::vector<std::pair<glm::vec3, glm::quat>> positions;
   positions.reserve(count);

   const auto xframe_array = xframes.read_array<Xframe>(count);

   for (const auto& xframe : xframe_array) {
      positions.emplace_back(read_xframe(xframe));
   }

   return positions;
}

void add_bones(const std::vector<std::string_view>& names,
               const std::vector<std::string_view>& parents,
               const std::vector<std::pair<glm::vec3, glm::quat>>& positions,
               msh::Builder& builder)
{
   for (std::size_t i = 0; i < names.size(); ++i) {
      msh::Bone bone;

      bone.name = names[i];
      bone.parent = parents[i];
      bone.position = positions[i].first;
      bone.rotation = positions[i].second;

      builder.add_bone(std::move(bone));
   }
}
}

void handle_skeleton(Ucfb_reader skeleton, msh::Builders_map& builders)
{
   auto info = skeleton.read_child_strict<"INFO"_mn>();

   const std::string name{info.read_string_unaligned()};
   const auto bone_count = info.read_trivial_unaligned<std::uint16_t>();

   std::vector<std::string_view> names =
      read_bone_names(skeleton.read_child_strict<"NAME"_mn>(), bone_count);

   std::vector<std::string_view> parents =
      read_bone_names(skeleton.read_child_strict<"PRNT"_mn>(), bone_count);

   std::vector<std::pair<glm::vec3, glm::quat>> positions =
      read_bone_xframes(skeleton.read_child_strict<"XFRM"_mn>(), bone_count);

   add_bones(names, parents, positions, builders[name]);
}