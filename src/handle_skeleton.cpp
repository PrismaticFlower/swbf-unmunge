
#include "chunk_headers.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"
#include "type_pun.hpp"

#define GLM_FORCE_CXX98
#include "glm/glm.hpp"

namespace {

#pragma pack(push, 1)

struct Xframe {
   glm::mat3 matrix;

   static_assert(std::is_standard_layout_v<glm::mat3>);
   static_assert(sizeof(glm::mat3) == 36);

   glm::vec3 position;

   static_assert(std::is_standard_layout_v<glm::vec3>);
   static_assert(sizeof(glm::vec3) == 12);
};

static_assert(std::is_standard_layout_v<Xframe>);
static_assert(sizeof(Xframe) == 48);

#pragma pack(pop)

struct Str_array {
   Magic_number mn;
   std::uint32_t size;
   char strs[];
};

static_assert(std::is_standard_layout_v<Str_array>);
static_assert(sizeof(Str_array) == 8);

struct Xframe_array {
   Magic_number mn;
   std::uint32_t size;

   Xframe xframes[];
};

static_assert(std::is_standard_layout_v<Xframe_array>);
static_assert(sizeof(Xframe_array) == 8);

auto read_xframe(const Xframe& xframe) -> std::pair<glm::vec3, glm::quat>
{
   return {xframe.position, glm::quat_cast(xframe.matrix)};
}

auto read_str_array(const Str_array& names, std::size_t count)
   -> std::vector<std::string_view>
{
   std::vector<std::string_view> strings;
   strings.reserve(count);

   for (std::size_t i{0}, head{0}; i < count; ++i) {
      if (head > names.size) throw std::runtime_error{"bad string array in skeleton"};

      strings.emplace_back(&names.strs[head]);

      head += strings.back().length() + 1;
   }

   return strings;
}

auto read_xframe_array(const Xframe_array& xfrms, std::size_t count)
   -> std::vector<std::pair<glm::vec3, glm::quat>>
{
   std::vector<std::pair<glm::vec3, glm::quat>> positions;
   positions.reserve(count);

   for (std::size_t i{0}, head{0}; i < count; ++i) {
      if (head > xfrms.size) throw std::runtime_error{"bad xframe array in skeleton"};

      positions.emplace_back(read_xframe(xfrms.xframes[i]));

      head += sizeof(Xframe);
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

void handle_skeleton(const chunks::Skeleton& skel, msh::Builders_map& builders)
{
   const std::string name{reinterpret_cast<const char*>(&skel.bytes[0]),
                          skel.info_size - 5};

   std::uint32_t head = skel.info_size;
   const std::uint32_t end = skel.size - 8;

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };
   align_head();

   const std::uint16_t bone_count =
      view_type_as<std::uint16_t>(skel.bytes[name.length() + 1]);

   std::vector<std::string_view> names;
   std::vector<std::string_view> parents;
   std::vector<std::pair<glm::vec3, glm::quat>> positions;

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(skel.bytes[head]);

      if (chunk.mn == "NAME"_mn) {
         names = read_str_array(view_type_as<Str_array>(skel.bytes[head]), bone_count);
      }
      else if (chunk.mn == "PRNT"_mn) {
         parents = read_str_array(view_type_as<Str_array>(skel.bytes[head]), bone_count);
      }
      else if (chunk.mn == "XFRM"_mn) {
         positions =
            read_xframe_array(view_type_as<Xframe_array>(skel.bytes[head]), bone_count);
      }

      head += chunk.size + 8;
      align_head();
   }

   add_bones(names, parents, positions, builders[name]);
}