
#include "app_options.hpp"
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "type_pun.hpp"
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
   Magic_number mn;
   std::uint32_t size;

   struct {
      Magic_number mn;
      std::uint32_t size;
      std::uint32_t dx_format;
      std::uint16_t width;
      std::uint16_t height;
      std::uint16_t unknown;
      std::uint16_t mipmap_count;
      std::uint32_t unknown_1;
   } info;

   static_assert(sizeof(decltype(info)) == 24);

   struct {
      Magic_number mn;
      std::uint32_t size;
      std::uint32_t lvl_mn;
      std::uint32_t lvl_size;

      std::uint32_t info_mn;
      std::uint32_t info_size;
      std::uint32_t info_mipmap;
      std::uint32_t info_mipmap_size;
   } face;

   static_assert(sizeof(decltype(face)) == 32);

   struct {
      Magic_number mn;
      std::uint32_t size;

      Byte data[];
   } body;

   static_assert(sizeof(decltype(body)) == 8);
};

static_assert(sizeof(Format_info) == 72);

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

DirectX::DDS_HEADER format_info_to_dds_header(const Format_info& format_info)
{
   DirectX::DDS_HEADER dds_header{};
   dds_header.dwSize = 124;
   dds_header.dwFlags = (0x1 | 0x2 | 0x4 | 0x1000);
   dds_header.dwHeight = format_info.info.height;
   dds_header.dwWidth = format_info.info.width;
   dds_header.ddspf = d3d_to_dds_format(format_info.info.dx_format);
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

auto read_texture(const chunks::Texture& chunk)
   -> std::pair<std::string_view, DirectX::ScratchImage>
{
   std::string_view name{reinterpret_cast<const char*>(&chunk.bytes[0]),
                         chunk.name_size - 1};

   std::uint32_t head = chunk.name_size;

   if (head % 4 != 0) head += (4 - (head % 4));

   const auto& tex_formats =
      *reinterpret_cast<const chunks::Unknown*>(&chunk.bytes[head]);
   head += tex_formats.size + 8;

   const auto& format_info = view_type_as<Format_info>(chunk.bytes[head]);

   const auto dds_header = format_info_to_dds_header(format_info);

   std::string buffer;
   buffer.reserve(128 + format_info.body.size);
   buffer += "DDS "s;
   buffer.resize(128 + format_info.body.size, '\0');

   std::memcpy(&buffer[4], &dds_header, sizeof(dds_header));
   std::memcpy(&buffer[128], &format_info.body.data[0], format_info.body.size);

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
   const auto& chunk = texture.view_as_chunk<chunks::Texture>();

   std::string_view name;
   DirectX::ScratchImage image;

   std::tie(name, image) = read_texture(chunk);

   save_image(name, std::move(image), file_saver, save_format);
}