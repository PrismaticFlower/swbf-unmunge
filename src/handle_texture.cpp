
#include "app_options.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
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

bool is_image_compressed(const DirectX::ScratchImage& image) noexcept
{
   const auto format = image.GetMetadata().format;

   if (format >= DXGI_FORMAT_BC1_TYPELESS && format <= DXGI_FORMAT_BC5_SNORM) {
      return true;
   }

   if (format >= DXGI_FORMAT_BC6H_TYPELESS && format <= DXGI_FORMAT_BC7_UNORM_SRGB) {
      return true;
   }

   return false;
}

bool image_needs_converting(const DirectX::ScratchImage& image) noexcept
{
   const auto format = image.GetMetadata().format;

   switch (format) {
   case DXGI_FORMAT_R8G8B8A8_UNORM:
      return false;
   case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      return false;
   case DXGI_FORMAT_B8G8R8A8_UNORM:
      return false;
   case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      return false;
   case DXGI_FORMAT_B8G8R8X8_UNORM:
      return false;
   case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      return false;
   default:
      return true;
   }
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
   const auto pixels = body.read_array<std::uint8_t>(body.size());

   const auto dds_header = create_dds_header(format_info);

   std::string buffer;
   buffer.reserve(128 + pixels.size());
   buffer += "DDS "s;
   buffer += view_pod_as_string(dds_header);
   buffer += view_pod_span_as_string(pixels);

   DirectX::ScratchImage image;
   DirectX::LoadFromDDSMemory(buffer.data(), buffer.size(), DirectX::DDS_FLAGS_NONE,
                              nullptr, image);

   return std::make_pair(std::move(name), std::move(image));
}

void ensure_basic_format(DirectX::ScratchImage& image)
{
   DirectX::ScratchImage conv_image;

   if (is_image_compressed(image)) {
      DirectX::Decompress(*image.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM,
                          conv_image);

      image = std::move(conv_image);
   }
   else if (image_needs_converting(image)) {
      DirectX::Convert(*image.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM,
                       DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT,
                       conv_image);

      image = std::move(conv_image);
   }
}

void save_dds_image(std::string name, DirectX::ScratchImage image, File_saver& file_saver)
{
   DirectX::Blob blob;

   DirectX::SaveToDDSMemory(*image.GetImage(0, 0, 0), DirectX::DDS_FLAGS_NONE, blob);

   std::string buffer{static_cast<const char*>(blob.GetBufferPointer()),
                      blob.GetBufferSize()};

   file_saver.save_file(std::move(buffer), name + ".dds"s, "textures");
}

void save_tga_image(std::string name, DirectX::ScratchImage image, File_saver& file_saver)
{
   ensure_basic_format(image);

   DirectX::Blob blob;

   DirectX::SaveToTGAMemory(*image.GetImage(0, 0, 0), blob);

   std::string buffer{static_cast<const char*>(blob.GetBufferPointer()),
                      blob.GetBufferSize()};

   file_saver.save_file(std::move(buffer), name + ".tga"s, "textures");
}

void save_png_image(std::string name, DirectX::ScratchImage image, File_saver& file_saver)
{
   ensure_basic_format(image);

   DirectX::Blob blob;

   DirectX::SaveToWICMemory(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
                            DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), blob);

   std::string buffer{static_cast<const char*>(blob.GetBufferPointer()),
                      blob.GetBufferSize()};

   file_saver.save_file(std::move(buffer), name + ".png"s, "textures");
}

void save_image(std::string_view name, DirectX::ScratchImage image,
                File_saver& file_saver, Image_format save_format)
{
   if (save_format == Image_format::tga) {
      save_tga_image(std::string{name}, std::move(image), file_saver);
   }
   else if (save_format == Image_format::png) {
      save_png_image(std::string{name}, std::move(image), file_saver);
   }
   else if (save_format == Image_format::dds) {
      save_dds_image(std::string{name}, std::move(image), file_saver);
   }
}
}

void handle_texture(Ucfb_reader texture, File_saver& file_saver, Image_format save_format)
{
   std::string_view name;
   DirectX::ScratchImage image;

   std::tie(name, image) = read_texture(Ucfb_reader_strict<"tex_"_mn>{texture});

   save_image(name, std::move(image), file_saver, save_format);
}