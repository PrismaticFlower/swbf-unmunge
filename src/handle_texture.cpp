
#include "app_options.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "save_image.hpp"
#include "ucfb_reader.hpp"

#include "DDS.h"
#include "DirectXTex.h"

#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <D3D9Types.h>

using namespace std::literals;

namespace {

struct Format_info {
   std::uint32_t dx_format;
   std::uint16_t width;
   std::uint16_t height;
   std::uint16_t unknown;
   std::uint16_t mipmap_count;
   std::uint32_t unknown_1;
};

static_assert(sizeof(Format_info) == 16);

DirectX::DDS_PIXELFORMAT d3d_to_dds_format(std::uint32_t format)
{
   switch (format) {
   case D3DFMT_DXT1:
      return DirectX::DDSPF_DXT1;
   case D3DFMT_DXT2:
      return DirectX::DDSPF_DXT2;
   case D3DFMT_DXT3:
      return DirectX::DDSPF_DXT3;
   case D3DFMT_DXT4:
      return DirectX::DDSPF_DXT4;
   case D3DFMT_DXT5:
      return DirectX::DDSPF_DXT5;
   case D3DFMT_R8G8_B8G8:
      return DirectX::DDSPF_R8G8_B8G8;
   case D3DFMT_G8R8_G8B8:
      return DirectX::DDSPF_G8R8_G8B8;
   case D3DFMT_YUY2:
      return DirectX::DDSPF_YUY2;
   case D3DFMT_A8R8G8B8:
      return DirectX::DDSPF_A8R8G8B8;
   case D3DFMT_X8R8G8B8:
      return DirectX::DDSPF_X8R8G8B8;
   case D3DFMT_A8B8G8R8:
      return DirectX::DDSPF_A8B8G8R8;
   case D3DFMT_X8B8G8R8:
      return DirectX::DDSPF_X8B8G8R8;
   case D3DFMT_G16R16:
      return DirectX::DDSPF_G16R16;
   case D3DFMT_R5G6B5:
      return DirectX::DDSPF_R5G6B5;
   case D3DFMT_A1R5G5B5:
      return DirectX::DDSPF_A1R5G5B5;
   case D3DFMT_A4R4G4B4:
      return DirectX::DDSPF_A4R4G4B4;
   case D3DFMT_R8G8B8:
      return DirectX::DDSPF_R8G8B8;
   case D3DFMT_L8:
      return DirectX::DDSPF_L8;
   case D3DFMT_L16:
      return DirectX::DDSPF_L16;
   case D3DFMT_A8L8:
      return DirectX::DDSPF_A8L8;
   case D3DFMT_A8:
      return DirectX::DDSPF_A8;
   case D3DFMT_V8U8:
      return DirectX::DDSPF_V8U8;
   case D3DFMT_Q8W8V8U8:
      return DirectX::DDSPF_Q8W8V8U8;
   case D3DFMT_V16U16:
      return DirectX::DDSPF_V16U16;
   default:
      throw std::runtime_error{"Texture has unknown format."};
   }
}

DirectX::DDS_HEADER create_dds_header(Format_info format_info)
{
   DirectX::DDS_HEADER dds_header{};
   dds_header.dwSize = 124;
   dds_header.dwFlags = (0x1 | 0x2 | 0x4 | 0x1000);
   dds_header.dwHeight = format_info.height;
   dds_header.dwWidth = format_info.width;
   dds_header.ddspf = d3d_to_dds_format(format_info.dx_format);
   dds_header.dwCaps = 0x1000;

   return dds_header;
}

auto read_texture(Ucfb_reader_strict<"tex_"_mn> texture)
   -> std::pair<std::string_view, DirectX::ScratchImage>
{
   const auto name = texture.read_child_strict<"NAME"_mn>().read_string();

   texture.read_child_strict<"INFO"_mn>(); // array of formats used by the texture

   // there are actually as many FMT_ chunks as the number of formats referenced in
   // the INFO chunk after NAME. But we'll only ever end up using one and the highest
   // quality format seems to come first so we just use that one.
   auto format = texture.read_child_strict<"FMT_"_mn>();

   const auto format_info =
      format.read_child_strict<"INFO"_mn>().read_trivial<Format_info>();

   // once again we (perhaps incorrectly) only care about the first face
   auto face = format.read_child_strict<"FACE"_mn>();
   // and the first mipmap
   auto mipmap_level = face.read_child_strict<"LVL_"_mn>();
   mipmap_level.read_child_strict<"INFO"_mn>();

   auto body = mipmap_level.read_child_strict<"BODY"_mn>();
   const auto pixels = body.read_bytes(body.size());

   const auto dds_header = create_dds_header(format_info);

   std::string buffer;
   buffer.reserve(128 + pixels.size());
   buffer += "DDS "s;
   buffer += view_object_as_string(dds_header);
   buffer += view_object_span_as_string(pixels);

   DirectX::ScratchImage image;
   DirectX::LoadFromDDSMemory(buffer.data(), buffer.size(), DirectX::DDS_FLAGS_NONE,
                              nullptr, image);

   return std::make_pair(std::move(name), std::move(image));
}
}

void handle_texture(Ucfb_reader texture, File_saver& file_saver, Image_format save_format,
                    Model_format model_format)
{
   std::string_view name;
   DirectX::ScratchImage image;

   std::tie(name, image) = read_texture(Ucfb_reader_strict<"tex_"_mn>{texture});

   save_image(name, std::move(image), file_saver, save_format, model_format);
}
