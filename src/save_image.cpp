
#include "app_options.hpp"
#include "file_saver.hpp"
#include "save_image_tga.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"

#include <exception>
#include <fstream>
#include <string_view>

#include <DirectXTex.h>
#include <fmt/format.h>

using namespace std::literals;

namespace {

void save_option_file(const DirectX::ScratchImage& image,
                      std::filesystem::path path) noexcept
{
   const bool cubemap = image.GetMetadata().IsCubemap();
   const bool volume = image.GetMetadata().IsVolumemap();

   if (!cubemap && !volume) return;

   path += L".option"sv;

   std::ofstream out{path};

   if (!out) {
      synced_cout::print(fmt::format("Unable create .option file {}"sv, path.string()));

      return;
   }

   if (cubemap) out << "-cubemap "sv;

   if (volume) out << "-volume "sv;
}

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
      DirectX::Decompress(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
                          DXGI_FORMAT_R8G8B8A8_UNORM, conv_image);

      image = std::move(conv_image);
   }
   else if (image_needs_converting(image)) {
      DirectX::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
                       DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_FORCE_NON_WIC,
                       DirectX::TEX_THRESHOLD_DEFAULT, conv_image);

      image = std::move(conv_image);
   }
}

auto unfold_cubemap(DirectX::ScratchImage image) -> DirectX::ScratchImage
{
   constexpr std::array<std::array<std::size_t, 2>, 6> face_offsets{
      {{2, 1}, {0, 1}, {1, 0}, {1, 2}, {1, 1}, {3, 1}}};

   DirectX::ScratchImage flat_image;
   flat_image.Initialize2D(image.GetMetadata().format, image.GetMetadata().width * 4,
                           image.GetMetadata().height * 3, 1, 1);

   for (std::size_t i = 0; i < 6; ++i) {
      auto face = *image.GetImage(0, i, 0);

      DirectX::CopyRectangle(
         face, {0, 0, face.width, face.height}, *flat_image.GetImage(0, 0, 0),
         DirectX::TEX_FILTER_FORCE_NON_WIC, face_offsets[i][0] * face.width,
         face_offsets[i][1] * face.height);
   }

   return flat_image;
}

auto separate_3d_texture(DirectX::ScratchImage image) -> DirectX::ScratchImage
{
   DirectX::ScratchImage flat_image;
   flat_image.Initialize2D(image.GetMetadata().format, image.GetMetadata().width,
                           image.GetMetadata().height * image.GetMetadata().depth, 1, 1);

   for (std::size_t z = 0; z < image.GetMetadata().depth; ++z) {
      auto slice = *image.GetImage(0, 0, z);

      DirectX::CopyRectangle(
         slice, {0, 0, slice.width, slice.height}, *flat_image.GetImage(0, 0, 0),
         DirectX::TEX_FILTER_FORCE_NON_WIC, 0, z * image.GetMetadata().depth);
   }

   return flat_image;
}

void ensure_flat_image(DirectX::ScratchImage& image)
{
   if (image.GetMetadata().IsCubemap()) {
      image = unfold_cubemap(std::move(image));
   }
   else if (image.GetMetadata().IsVolumemap()) {
      image = separate_3d_texture(std::move(image));
   }
}

auto image_extension(const Image_format format) noexcept -> std::string_view
{
   switch (format) {
   case Image_format::tga:
      return ".tga"sv;
   case Image_format::png:
      return ".png"sv;
   case Image_format::dds:
      return ".dds"sv;
   default:
      std::terminate();
   }
}

}

void save_image(std::string_view name, DirectX::ScratchImage image,
                File_saver& file_saver, Image_format save_format,
                Model_format model_format)
{
   if (get_pre_processing_global()) return; // ---------early return----------------
   // Windows' 3D Viewer doesn't handle relative texture paths, so we have to put the
   // textures in the same folder as the glTF files if we want them to be previewable in
   // it.
   const auto dir = model_format == Model_format::gltf2 ? "models"sv : "textures"sv;

   // glTF doesn't support .tga files.
   save_format = model_format == Model_format::gltf2 ? Image_format::png : save_format;

   const auto path = file_saver.build_file_path(dir, name, image_extension(save_format));

   file_saver.create_dir(dir);

   if (save_format == Image_format::tga) {
      save_option_file(image, path);

      ensure_basic_format(image);
      ensure_flat_image(image);

      save_image_tga(path, *image.GetImage(0, 0, 0));
   }
   else if (save_format == Image_format::png) {
      ensure_basic_format(image);
      ensure_flat_image(image);

      DirectX::SaveToWICFile(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
                             DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str());
   }
   else if (save_format == Image_format::dds) {
      DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(),
                             image.GetMetadata(), DirectX::DDS_FLAGS_NONE, path.c_str());
   }
}
