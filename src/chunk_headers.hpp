#pragma once

#include "byte.hpp"
#include "magic_number.hpp"

#include <cstdint>
#include <type_traits>

#pragma warning(disable : 4200)

namespace chunks {

namespace impl {

struct Chunk_base {
   Chunk_base() = delete;

   Chunk_base(const Chunk_base&) = delete;
   Chunk_base& operator=(const Chunk_base&) = delete;

   Chunk_base(Chunk_base&&) = delete;
   Chunk_base& operator=(Chunk_base&&) = delete;
};
}

#pragma pack(push, 1)

struct Unknown : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
};

static_assert(std::is_standard_layout_v<Unknown>);
static_assert(sizeof(Unknown) == 8);

struct Ucfb : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Ucfb>);
static_assert(sizeof(Ucfb) == 8);

struct Child_lvl : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   std::uint64_t name;
   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Child_lvl>);
static_assert(sizeof(Child_lvl) == 16);

struct Object : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Object>);
static_assert(sizeof(Object) == 8);

struct Config : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;

   std::uint64_t name_meta;
   alignas(4) std::uint32_t name_hash;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Config>);
static_assert(sizeof(Config) == 20);

struct Texture : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;

   std::uint32_t name_mn;
   std::uint32_t name_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Texture>);
static_assert(sizeof(Texture) == 16);

struct World : impl::Chunk_base {
   std::uint32_t mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<World>);
static_assert(sizeof(World) == 8);

struct Planning : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;

   struct {
      Magic_number mn;
      std::uint32_t size;
      std::uint16_t hub_count;
      std::uint16_t arc_count;
      std::uint16_t unknown;
   } info;

   Byte _padding[2];

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Planning>);
static_assert(sizeof(Planning) == 24);

struct Path : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Path>);
static_assert(sizeof(Path) == 8);

struct Localization : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t name_mn;
   std::uint32_t name_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Localization>);
static_assert(sizeof(Localization) == 16);

struct Terrain : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t name_mn;
   std::uint32_t name_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Terrain>);
static_assert(sizeof(Terrain) == 16);

struct Model : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t name_mn;
   std::uint32_t name_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Model>);
static_assert(sizeof(Model) == 16);

struct Skeleton : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t info_mn;
   std::uint32_t info_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Skeleton>);
static_assert(sizeof(Skeleton) == 16);

struct Collision : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t name_mn;
   std::uint32_t name_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Collision>);
static_assert(sizeof(Collision) == 16);

struct Primitives : impl::Chunk_base {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t info_mn;
   std::uint32_t info_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Primitives>);
static_assert(sizeof(Primitives) == 16);

#pragma pack(pop)
}