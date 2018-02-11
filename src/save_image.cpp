
#include "app_options.hpp"
#include "file_saver.hpp"
#include "string_helpers.hpp"

#include "DirectXTex.h"

namespace {

bool image_needs_converting(const DirectX::ScratchImage& image) noexcept
{
   const auto format = image.GetMetadata().format;

   switch (format) {
   case DXGI_FORMAT_R8G8B8A8_UNORM:
   case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
   case DXGI_FORMAT_B8G8R8A8_UNORM:
   case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
   case DXGI_FORMAT_B8G8R8X8_UNORM:
   case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      return false;
   default:
      return true;
   }
}

void ensure_basic_format(DirectX::ScratchImage& image)
{
   DirectX::ScratchImage conv_image;

   if (DirectX::IsCompressed(image.GetMetadata().format)) {
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

void save_dds_image(std::string_view name, DirectX::ScratchImage image,
                    File_saver& file_saver)
{
   DirectX::Blob blob;

   DirectX::SaveToDDSMemory(*image.GetImage(0, 0, 0), DirectX::DDS_FLAGS_NONE, blob);

   const std::string_view buffer{static_cast<const char*>(blob.GetBufferPointer()),
                                 blob.GetBufferSize()};

   file_saver.save_file(buffer, "textures"_sv, name, ".dds"_sv);
}

void save_tga_image(std::string_view name, DirectX::ScratchImage image,
                    File_saver& file_saver)
{
   ensure_basic_format(image);

   DirectX::Blob blob;

   DirectX::SaveToTGAMemory(*image.GetImage(0, 0, 0), blob);

   const std::string_view buffer{static_cast<const char*>(blob.GetBufferPointer()),
                                 blob.GetBufferSize()};

   file_saver.save_file(buffer, "textures"_sv, name, ".tga"_sv);
}

void save_png_image(std::string_view name, DirectX::ScratchImage image,
                    File_saver& file_saver)
{
   ensure_basic_format(image);

   DirectX::Blob blob;

   DirectX::SaveToWICMemory(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
                            DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), blob);

   const std::string_view buffer{static_cast<const char*>(blob.GetBufferPointer()),
                                 blob.GetBufferSize()};

   file_saver.save_file(buffer, "textures"_sv, name, ".png"_sv);
}
}

void save_image(std::string_view name, DirectX::ScratchImage image,
                File_saver& file_saver, Image_format save_format)
{
   if (save_format == Image_format::tga) {
      save_tga_image(name, std::move(image), file_saver);
   }
   else if (save_format == Image_format::png) {
      save_png_image(name, std::move(image), file_saver);
   }
   else if (save_format == Image_format::dds) {
      save_dds_image(name, std::move(image), file_saver);
   }
}
