
#include "app_options.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "save_image.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include <DirectXTex.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <D3D9Types.h>

using namespace std::literals;

namespace {

enum class Texture_type { _2d = 1, cube = 2, _3d = 3 };

struct Badformat_exception : std::runtime_error {
   using std::runtime_error::runtime_error;
};

struct Texture_info {
   D3DFORMAT format;
   std::uint16_t width;
   std::uint16_t height;
   std::uint16_t depth;
   std::uint16_t mipmap_count;
   std::uint32_t type_detail_bias;
};

static_assert(sizeof(Texture_info) == 16);

static_assert(sizeof(D3DFORMAT) == sizeof(std::uint32_t));

constexpr std::array format_rankings{D3DFMT_A32B32G32R32F,
                                     D3DFMT_A16B16G16R16F,
                                     D3DFMT_A16B16G16R16,
                                     D3DFMT_A2R10G10B10,
                                     D3DFMT_A2B10G10R10,
                                     D3DFMT_A2B10G10R10_XR_BIAS,
                                     D3DFMT_A8R8G8B8,
                                     D3DFMT_X8R8G8B8,
                                     D3DFMT_A8B8G8R8,
                                     D3DFMT_X8B8G8R8,
                                     D3DFMT_R8G8B8,

                                     D3DFMT_G16R16F,
                                     D3DFMT_G16R16,
                                     D3DFMT_A8L8,

                                     D3DFMT_G32R32F,
                                     D3DFMT_R32F,
                                     D3DFMT_R16F,
                                     D3DFMT_L16,
                                     D3DFMT_A8,
                                     D3DFMT_L8,

                                     D3DFMT_R5G6B5,
                                     D3DFMT_X1R5G5B5,
                                     D3DFMT_A1R5G5B5,

                                     D3DFMT_R8G8_B8G8,
                                     D3DFMT_G8R8_G8B8,

                                     D3DFMT_Q16W16V16U16,
                                     D3DFMT_A2W10V10U10,
                                     D3DFMT_X8L8V8U8,
                                     D3DFMT_Q8W8V8U8,
                                     D3DFMT_L6V5U5,

                                     D3DFMT_V16U16,
                                     D3DFMT_V8U8,

                                     D3DFMT_UYVY,
                                     D3DFMT_YUY2};

auto d3d_to_dxgi_format(const D3DFORMAT format) -> DXGI_FORMAT
{
   switch (format) {
   case D3DFMT_A8R8G8B8:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
   case D3DFMT_X8R8G8B8:
      return DXGI_FORMAT_B8G8R8X8_UNORM;
   case D3DFMT_R5G6B5:
      return DXGI_FORMAT_B5G6R5_UNORM;
   case D3DFMT_A1R5G5B5:
   case D3DFMT_X1R5G5B5:
      return DXGI_FORMAT_B5G5R5A1_UNORM;
   case D3DFMT_A4R4G4B4:
      return DXGI_FORMAT_B4G4R4A4_UNORM;
   case D3DFMT_A8:
      return DXGI_FORMAT_A8_UNORM;
   case D3DFMT_A2B10G10R10:
      return DXGI_FORMAT_R10G10B10A2_UNORM;
   case D3DFMT_A8B8G8R8:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
   case D3DFMT_G16R16:
      return DXGI_FORMAT_R16G16_UNORM;
   case D3DFMT_A16B16G16R16:
      return DXGI_FORMAT_R16G16B16A16_UNORM;
   case D3DFMT_V8U8:
      return DXGI_FORMAT_R8G8_SNORM;
   case D3DFMT_Q8W8V8U8:
      return DXGI_FORMAT_R8G8B8A8_SNORM;
   case D3DFMT_V16U16:
      return DXGI_FORMAT_R16G16_SNORM;
   case D3DFMT_R8G8_B8G8:
      return DXGI_FORMAT_G8R8_G8B8_UNORM;
   case D3DFMT_G8R8_G8B8:
      return DXGI_FORMAT_R8G8_B8G8_UNORM;
   case D3DFMT_DXT1:
      return DXGI_FORMAT_BC1_UNORM;
   case D3DFMT_DXT2:
   case D3DFMT_DXT3:
      return DXGI_FORMAT_BC2_UNORM;
   case D3DFMT_DXT4:
   case D3DFMT_DXT5:
      return DXGI_FORMAT_BC3_UNORM;
   case D3DFMT_D16_LOCKABLE:
   case D3DFMT_D16:
      return DXGI_FORMAT_D16_UNORM;
   case D3DFMT_D24S8:
      return DXGI_FORMAT_D24_UNORM_S8_UINT;
   case D3DFMT_D32:
   case D3DFMT_D32F_LOCKABLE:
      return DXGI_FORMAT_D32_FLOAT;
   case D3DFMT_Q16W16V16U16:
      return DXGI_FORMAT_R16G16B16A16_SNORM;
   case D3DFMT_R16F:
      return DXGI_FORMAT_R16_FLOAT;
   case D3DFMT_G16R16F:
      return DXGI_FORMAT_R16G16_FLOAT;
   case D3DFMT_A16B16G16R16F:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
   case D3DFMT_R32F:
      return DXGI_FORMAT_R32_FLOAT;
   case D3DFMT_G32R32F:
      return DXGI_FORMAT_R32G32_FLOAT;
   case D3DFMT_A32B32G32R32F:
      return DXGI_FORMAT_R32G32B32A32_FLOAT;
   case D3DFMT_A2B10G10R10_XR_BIAS:
      return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;
   case D3DFMT_L8:
      return DXGI_FORMAT_R8_UNORM;
   case D3DFMT_A8L8:
      return DXGI_FORMAT_R8G8_UNORM;
   case D3DFMT_L16:
      return DXGI_FORMAT_R16_UNORM;
   default:
      throw std::runtime_error{"Texture has unknown or unsupported format."};
   }
}

auto create_scratch_image(const Texture_info& info) -> DirectX::ScratchImage
{
   const Texture_type type{info.type_detail_bias & 0xffu};
   [[maybe_unused]] std::uint32_t detail_bias = info.type_detail_bias >> 8u;

   DirectX::ScratchImage image;

   if (type == Texture_type::_2d) {
      if (FAILED(image.Initialize2D(d3d_to_dxgi_format(info.format), info.width,
                                    info.height, 1, info.mipmap_count))) {
         throw Badformat_exception{"bad format"};
      }
   }
   else if (type == Texture_type::cube) {
      if (FAILED(image.InitializeCube(d3d_to_dxgi_format(info.format), info.width,
                                      info.height, 1, info.mipmap_count))) {
         throw Badformat_exception{"bad format"};
      }
   }
   else if (type == Texture_type::_3d) {
      if (FAILED(image.Initialize3D(d3d_to_dxgi_format(info.format), info.width,
                                    info.height, info.depth, info.mipmap_count))) {
         throw Badformat_exception{"bad format"};
      }
   }
   else {
      throw Badformat_exception{"bad format"};
   }

   return image;
}

bool is_luminance_format(const D3DFORMAT format)
{
   switch (format) {
   case D3DFMT_L8:
   case D3DFMT_A8L8:
   case D3DFMT_L16:
   case D3DFMT_A4L4:
      return true;
   }

   return false;
}

auto sort_formats(std::vector<D3DFORMAT> formats) -> std::vector<D3DFORMAT>
{
   std::sort(std::begin(formats), std::end(formats),
             [](const D3DFORMAT l, const D3DFORMAT r) {
                const auto rank = [](const D3DFORMAT f) {
                   return std::distance(
                      format_rankings.begin(),
                      std::find(format_rankings.cbegin(), format_rankings.cend(), f));
                };

                return rank(l) < rank(r);
             });

   return formats;
}

auto patch_luminance_format(DirectX::ScratchImage input, const D3DFORMAT format)
   -> DirectX::ScratchImage
{
   DirectX::ScratchImage result;

   if (FAILED(DirectX::Convert(*input.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM,
                               DirectX::TEX_FILTER_DEFAULT, 0.5f, result))) {
      synced_cout::print(
         "Warning failed to convert luminance format texture. "
         "The texture's contents will be intact but it's colour channels will need fixing up manually in an editor."sv);

      return input;
   }

   for (std::size_t i = 0; i < result.GetImageCount(); ++i) {
      auto image = result.GetImages()[i];

      for (int y = 0; y < image.height; ++y) {
         for (int x = 0; x < image.width; ++x) {
            std::uint8_t* pixel_address =
               image.pixels + (y * image.rowPitch) + (x * sizeof(std::uint32_t));
            std::uint32_t pixel;

            std::memcpy(&pixel, pixel_address, sizeof(std::uint32_t));

            if (format == D3DFMT_L8 || format == D3DFMT_L16) {
               const std::uint32_t l = pixel & 0xff;

               pixel = l;
               pixel |= (l << 8);
               pixel |= (l << 16);
               pixel |= (0xff << 24);
            }
            else if (format == D3DFMT_A8L8 || format == D3DFMT_A4L4) {
               const std::uint32_t l = pixel & 0xff;
               const std::uint32_t a = (pixel & 0xff00) >> 8;

               pixel = l;
               pixel |= (l << 8);
               pixel |= (l << 16);
               pixel |= (a << 24);
            }

            std::memcpy(pixel_address, &pixel, sizeof(std::uint32_t));
         }
      }
   }

   return result;
}

auto read_format_list(Ucfb_reader_strict<"INFO"_mn> info) -> std::vector<D3DFORMAT>
{
   const auto count = info.read_trivial<std::uint32_t>();

   return info.read_array<D3DFORMAT>(count);
}

auto read_texture_format(Ucfb_reader_strict<"tex_"_mn> texture, const D3DFORMAT format)
   -> DirectX::ScratchImage
{
   auto fmt = [&] {
      while (texture) {
         auto fmt = texture.read_child_strict_optional<"FMT_"_mn>();

         if (!fmt) throw Badformat_exception{"bad format"};

         const auto texture_info =
            fmt->read_child_strict<"INFO"_mn>().read_trivial<Texture_info>();

         fmt->reset_head();

         if (texture_info.format == format) return *fmt;
      }
   }();

   const auto info = fmt.read_child_strict<"INFO"_mn>().read_trivial<Texture_info>();

   auto scratch_image = create_scratch_image(info);

   const std::size_t face_count = scratch_image.GetMetadata().IsCubemap() ? 6 : 1;

   for (std::size_t face_index = 0; face_index < face_count; ++face_index) {
      auto face = fmt.read_child_strict<"FACE"_mn>();
      for (std::size_t mip_index = 0; mip_index < info.mipmap_count; ++mip_index) {
         auto lvl = face.read_child_strict<"LVL_"_mn>();

         const auto [mip_level, body_size] =
            lvl.read_child_strict<"INFO"_mn>().read_multi<std::uint32_t, std::uint32_t>();

         const std::size_t depth = std::max(info.depth >> mip_level, 1);
         auto image = scratch_image.GetImage(mip_level, face_index, 0);

         auto body = lvl.read_child_strict<"BODY"_mn>();
         body.read_array_to_span(
            body_size, gsl::make_span(image->pixels, image->slicePitch * depth));
      }
   }

   if (is_luminance_format(format)) {
      return patch_luminance_format(std::move(scratch_image), format);
   }

   return scratch_image;
}

auto read_texture(Ucfb_reader_strict<"tex_"_mn> texture)
   -> std::pair<std::string, DirectX::ScratchImage>
{
   const auto name = texture.read_child_strict<"NAME"_mn>().read_string();

   const auto formats =
      sort_formats(read_format_list(texture.read_child_strict<"INFO"_mn>()));

   for (const auto format : formats) {
      try {
         return {std::string{name}, read_texture_format(texture, format)};
      }
      catch (Badformat_exception&) {
      }
   }

   throw std::runtime_error{fmt::format("Texture {} has no usable formats!"sv, name)};
}
}

void handle_texture(Ucfb_reader texture, File_saver& file_saver, Image_format save_format,
                    Model_format model_format)
{
   auto [name, image] = read_texture(Ucfb_reader_strict<"tex_"_mn>{texture});

   save_image(name, std::move(image), file_saver, save_format, model_format);
}
