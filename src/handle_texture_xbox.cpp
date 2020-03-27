
#include "DDS.h"
#include "app_options.hpp"
#include "file_saver.hpp"
#include "save_image.hpp"
#include "ucfb_reader.hpp"

#include <DirectXTex.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include <d3d9types.h>

using namespace std::literals;

namespace {

enum class Xbox_format : std::uint32_t {
   l8 = 0,

   a4l4_or_a1r5g5b5 = 1,

   a4r4g4b4 = 4,
   r5g6b5 = 5,
   a8r8g8b8 = 6,

   dxt1 = 12,
   dxt3 = 14,

   a8l8 = 26,
   a8 = 25,

   u8v8 = 40,
};

enum class Texture_type : std::uint32_t {
   t_2d = 1,
   t_cube = 2,
   t_3d = 3,
};

struct Texture_info {
   std::uint16_t width;
   std::uint16_t height;
   std::uint16_t depth;
   std::uint16_t mipcount;
   Texture_type type;
   Xbox_format format;
   std::uint32_t body_size;
};

const DirectX::DDS_PIXELFORMAT pixel_format_a4l4 = {
   sizeof(DirectX::DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x0f, 0x00, 0x00, 0xf0};

// DDS_LUMINANCEA, 0, 16, 0x00ff, 0x0000, 0x0000, 0xff00

static_assert(sizeof(Texture_info) == 20);
static_assert(std::is_trivially_copyable_v<Texture_info>);

std::uint32_t get_8bit_mip_chain_size(const Texture_info& info)
{
   std::uint32_t size = 0;

   glm::uvec2 level{info.width, info.height};

   for (auto i = 0; i < int{info.mipcount}; ++i) {
      size += level.x * level.y;

      level /= 2u;
   }

   return size;
}

DirectX::DDS_PIXELFORMAT xbox_to_dds_format(const Texture_info& info)
{
   switch (info.format) {
   case Xbox_format::l8:
      return DirectX::DDSPF_L8;
   case Xbox_format::a4l4_or_a1r5g5b5:
      if (info.body_size == get_8bit_mip_chain_size(info)) {
         return pixel_format_a4l4;
      }
      else {
         return DirectX::DDSPF_A1R5G5B5;
      }
   case Xbox_format::a4r4g4b4:
      return DirectX::DDSPF_A4R4G4B4;
   case Xbox_format::r5g6b5:
      return DirectX::DDSPF_R5G6B5;
   case Xbox_format::a8r8g8b8:
      return DirectX::DDSPF_A8R8G8B8;
   case Xbox_format::dxt1:
      return DirectX::DDSPF_DXT1;
   case Xbox_format::dxt3:
      return DirectX::DDSPF_DXT3;
   case Xbox_format::a8l8:
      return DirectX::DDSPF_A8L8;
   case Xbox_format::a8:
      return DirectX::DDSPF_A8;
   case Xbox_format::u8v8:
      return DirectX::DDSPF_V8U8;
   default:
      throw std::runtime_error{"Texture has unknown format."};
   }
}

DirectX::DDS_HEADER create_dds_header(const Texture_info& info)
{
   DirectX::DDS_HEADER dds_header{};
   dds_header.dwSize = sizeof(DirectX::DDS_HEADER);
   dds_header.dwFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
   dds_header.dwHeight = info.height;
   dds_header.dwWidth = info.width;
   dds_header.dwMipMapCount = info.mipcount;
   dds_header.ddspf = xbox_to_dds_format(info);
   dds_header.dwCaps = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;

   return dds_header;
}

auto read_texture(Ucfb_reader_strict<"tex_"_mn> texture)
   -> std::pair<std::string_view, DirectX::ScratchImage>
{
   const auto name = texture.read_child_strict<"NAME"_mn>().read_string();
   const auto info = texture.read_child_strict<"INFO"_mn>().read_trivial<Texture_info>();
   const auto data =
      texture.read_child_strict<"BODY"_mn>().read_bytes_unaligned(info.body_size);

   if (info.type != Texture_type::t_2d) {
      throw std::runtime_error{"Skipping unsupported texture format (cubemap or 3D)."};
   }

   const auto dds_header = create_dds_header(info);

   std::string buffer;
   buffer.reserve(4 + sizeof(dds_header) + info.body_size);
   buffer += "DDS "sv;
   buffer += view_object_as_string(dds_header);
   buffer += view_object_span_as_string(data);

   DirectX::ScratchImage image;
   DirectX::LoadFromDDSMemory(buffer.data(), buffer.size(),
                              DirectX::DDS_FLAGS_BAD_DXTN_TAILS, nullptr, image);

   return {std::move(name), std::move(image)};
}
}

void handle_texture_xbox(Ucfb_reader texture, File_saver& file_saver,
                         Image_format save_format, Model_format model_format)
{
   auto [name, image] = read_texture(Ucfb_reader_strict<"tex_"_mn>{texture});

   save_image(name, std::move(image), file_saver, save_format, model_format);
}
