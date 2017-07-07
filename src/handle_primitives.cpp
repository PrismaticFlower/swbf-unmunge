
#include"chunk_headers.hpp"
#include"magic_number.hpp"
#include"msh_builder.hpp"
#include"type_pun.hpp"


namespace
{

#pragma pack(push, 1)

struct Mask
{
   Magic_number mn;
   std::uint32_t size;

   msh::Collision_flags flags;
};

static_assert(std::is_standard_layout_v<Mask>);
static_assert(sizeof(Mask) == 12);

struct Parent
{
   Magic_number mn;
   std::uint32_t size;

   char str[];

};

static_assert(std::is_standard_layout_v<Parent>);
static_assert(sizeof(Parent) == 8);

struct Xframe
{
   Magic_number mn;
   std::uint32_t size;

   glm::mat3 rotation;
   glm::vec3 position;
};

static_assert(std::is_standard_layout_v<Xframe>);
static_assert(sizeof(Xframe) == 56);

struct Data
{
   Magic_number mn;
   std::uint32_t size;
   msh::Primitive_type type;
   glm::vec3 prim_size;
};

static_assert(std::is_standard_layout_v<Data>);
static_assert(sizeof(Data) == 24);

#pragma pack(pop)

template<typename Type>
const Type& find_chunk(std::uint32_t& head, const std::uint32_t end,
                       const Byte* bytes, Magic_number mn)
{
   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(bytes[head]);

      head += chunk.size + 8;
      if (head % 4 != 0) head += (4 - (head % 4));

      if (chunk.mn == mn) return view_type_as<Type>(chunk);
   }

   throw std::runtime_error{"badly formed collision primitive"};
}

template<typename Type>
const Type* find_optional_chunk(std::uint32_t& head, const std::uint32_t end,
                                const Byte* bytes, Magic_number mn,
                                Magic_number end_at_mn)
{
   std::uint32_t local_head = head;

   while (local_head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(bytes[head]);

      local_head += chunk.size + 8;
      if (local_head % 4 != 0) local_head += (4 - (local_head % 4));

      if (chunk.mn == end_at_mn) return nullptr;
      if (chunk.mn == mn) {
         head = local_head;

         return &view_type_as<Type>(chunk);
      }
   }

   return nullptr;
}

auto read_primitive(std::uint32_t& head, const std::uint32_t end,
                    const chunks::Primitives& prim) -> msh::Collision_primitive
{
   msh::Collision_primitive msh_prim;

   find_chunk<chunks::Unknown>(head, end,
                               &prim.bytes[0], "NAME"_mn);

   const auto* mask = find_optional_chunk<Mask>(head, end,
                                                &prim.bytes[0], "MASK"_mn,
                                                "NAME"_mn);

   if (mask) msh_prim.flags = mask->flags;

   const auto& parent = find_chunk<Parent>(head, end,
                                           &prim.bytes[0], "PRNT"_mn);
   msh_prim.parent = std::string{&parent.str[0], parent.size - 1};

   const auto& xframe = find_chunk<Xframe>(head, end,
                                           &prim.bytes[0], "XFRM"_mn);

   msh_prim.rotation = glm::quat_cast(xframe.rotation);
   msh_prim.position = xframe.position;

   const auto& data = find_chunk<Data>(head, end,
                                       &prim.bytes[0], "DATA"_mn);

   msh_prim.type = data.type;
   msh_prim.size = data.prim_size;

   return msh_prim;
}

}

void handle_primitives(const chunks::Primitives& prim,
                       msh::Builders_map& builders)
{
   std::string name{reinterpret_cast<const char*>(&prim.bytes[0]), prim.info_size - 5};

   std::uint32_t head = prim.info_size;
   const std::uint32_t end = prim.size - 8;

   if (head % 4 != 0) head += (4 - (head % 4));

   auto& builder = builders[name];

   while (head < end) {
      builder.add_collision_primitive(read_primitive(head, end, prim));
   }
}