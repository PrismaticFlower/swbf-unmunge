#pragma once

#include "byte.hpp"
#include "magic_number.hpp"
#include "msh_builder.hpp"

#include <cstdint>
#include <type_traits>

enum class Vbuf_types : std::uint32_t {
   // xyz (position) - xyz (normal) - uv (texture coordinates)
   xyznuv = 0x00000222,
   // xyz (position) - xyz (normal) - rgba - uv (texture coordinates)
   xyzncuv = 0x0000322,
   // xyz (position) - xyz (normal) - rgba - uv (texture coordinates)
   xyzncuv_2 = 0x000002a2,

   // xyz (position) - skin - xyz (normal) - uv (texture coordinates)
   xyzsknuv = 0x0000226,

   unknown_16 = 0x0000d222,
   unknown_20 = 0x0000f226
};

#pragma pack(push, 1)

struct Vbuf {
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t entry_count;
   std::uint32_t entry_size;
   Vbuf_types type;
};

static_assert(std::is_standard_layout_v<Vbuf>);
static_assert(sizeof(Vbuf) == 20);

#pragma pack(pop)

void process_vbuf(const Vbuf& vbuf, msh::Model& model);
