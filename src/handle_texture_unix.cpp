
#include "app_options.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "save_image_unix.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <iostream>

#include <D3D9Types.h>

#include "image_converters_unix.h"

#define COUT(x) std::cout << x << std::endl;

using namespace std::literals;

namespace {

struct Badformat_exception : std::runtime_error {
   using std::runtime_error::runtime_error;
};

struct Texture_info {
   D3DFORMAT format;
   std::uint16_t width;
   std::uint16_t height;
   std::uint16_t depth;
   std::uint16_t mipmap_count;
   std::uint32_t type;
};


//static_assert(sizeof(Texture_info) == 16);

//static_assert(sizeof(D3DFORMAT) == sizeof(std::uint32_t));


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


std::string D3DToString(D3DFORMAT d3dFormat) {
                                  switch (d3dFormat) {
                                    case D3DFMT_A8R8G8B8:
                                        return "D3DFMT_A8R8G8B8";
                                    case D3DFMT_X8R8G8B8:
                                        return "D3DFMT_X8R8G8B8";
                                    case D3DFMT_R5G6B5:
                                        return "D3DFMT_R5G6B5";
                                    case D3DFMT_A1R5G5B5:
                                        return "D3DFMT_A1R5G5B5";
                                    case D3DFMT_X1R5G5B5:
                                        return "D3DFMT_X1R5G5B5";
                                    case D3DFMT_A4R4G4B4:
                                        return "D3DFMT_A4R4G4B4";
                                    case D3DFMT_A8:
                                        return "D3DFMT_A8";
                                    case D3DFMT_A2B10G10R10:
                                        return "D3DFMT_A2B10G10R10";
                                    case D3DFMT_A8B8G8R8:
                                        return "D3DFMT_A8B8G8R8";
                                    case D3DFMT_G16R16:
                                        return "D3DFMT_G16R16";
                                    case D3DFMT_A16B16G16R16:
                                        return "D3DFMT_A16B16G16R16";
                                    case D3DFMT_V8U8:
                                        return "D3DFMT_V8U8";
                                    case D3DFMT_Q8W8V8U8:
                                        return "D3DFMT_Q8W8V8U8";
                                    case D3DFMT_V16U16:
                                        return "D3DFMT_V16U16";
                                    case D3DFMT_R8G8_B8G8:
                                        return "D3DFMT_R8G8_B8G8";
                                    case D3DFMT_G8R8_G8B8:
                                        return "D3DFMT_G8R8_G8B8";
                                    case D3DFMT_DXT1:
                                        return "D3DFMT_DXT1";
                                    case D3DFMT_DXT2:
                                        return "D3DFMT_DXT2";
                                    case D3DFMT_DXT3:
                                        return "D3DFMT_DXT3";
                                    case D3DFMT_DXT4:
                                        return "D3DFMT_DXT4";
                                    case D3DFMT_DXT5:
                                        return "D3DFMT_DXT5";
                                    case D3DFMT_D16_LOCKABLE:
                                        return "D3DFMT_D16_LOCKABLE";
                                    case D3DFMT_D16:
                                        return "D3DFMT_D16";
                                    case D3DFMT_D24S8:
                                        return "D3DFMT_D24S8";
                                    case D3DFMT_D32:
                                        return "D3DFMT_D32";
                                    case D3DFMT_D32F_LOCKABLE:
                                        return "D3DFMT_D32F_LOCKABLE";
                                    case D3DFMT_Q16W16V16U16:
                                        return "D3DFMT_Q16W16V16U16";
                                    case D3DFMT_R16F:
                                        return "D3DFMT_R16F";
                                    case D3DFMT_G16R16F:
                                        return "D3DFMT_G16R16F";
                                    case D3DFMT_A16B16G16R16F:
                                        return "D3DFMT_A16B16G16R16F";
                                    case D3DFMT_R32F:
                                        return "D3DFMT_R32F";
                                    case D3DFMT_G32R32F:
                                        return "D3DFMT_G32R32F";
                                    case D3DFMT_A32B32G32R32F:
                                        return "D3DFMT_A32B32G32R32F";
                                    case D3DFMT_A2B10G10R10_XR_BIAS:
                                        return "D3DFMT_A2B10G10R10_XR_BIAS";
                                    case D3DFMT_L8:
                                        return "D3DFMT_L8";
                                    case D3DFMT_A8L8:
                                        return "D3DFMT_A8L8";
                                    case D3DFMT_L16:
                                        return "D3DFMT_L16";
                                    default:
                                        return fmt::format("Unknown Format: {}", d3dFormat).c_str();
                                  }
}

/*
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
*/

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

/*
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

   auto image = *result.GetImage(0, 0, 0);

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

   return result;
}
*/

auto read_format_list(Ucfb_reader_strict<"INFO"_mn> info) -> std::vector<D3DFORMAT>
{
   const auto count = info.read_trivial<std::uint32_t>();

   return info.read_array<D3DFORMAT>(count);
}

auto read_texture_format(Ucfb_reader_strict<"tex_"_mn> texture, const D3DFORMAT format)
   -> cv::Mat
{
   while (texture) {
      auto fmt = texture.read_child_strict_optional<"FMT_"_mn>();

      if (!fmt) throw Badformat_exception{"bad format"};

      const auto texture_info =
         fmt->read_child_strict<"INFO"_mn>().read_trivial<Texture_info>();

      if (texture_info.format != format) continue;

      // Pretend like 2D textures are the only thing that exist.
      auto lvl = fmt->read_child_strict<"FACE"_mn>().read_child_strict<"LVL_"_mn>();

      [[maybe_unused]] const auto [mip_level, bytes_size] =
         lvl.read_child_strict<"INFO"_mn>().read_multi<std::uint32_t, std::uint32_t>();

      cv::Mat image(texture_info.height, texture_info.width, CV_8UC3, cv::Scalar(100, 10, 100));
      unsigned char *pixelDump = new unsigned char[texture_info.height * texture_info.width * 8];


      auto body = lvl.read_child_strict<"BODY"_mn>();

      std::string imgInfo = fmt::format("Width {}, Height {}, Numbytes {}, Format {}",
                                        texture_info.width, texture_info.height, 
                                        body.size(), 
                                        D3DToString(texture_info.format));
      COUT(imgInfo)

      body.read_array_to_span(body.size(), gsl::make_span(pixelDump, body.size()));

      if (texture_info.format == D3DFMT_R5G6B5){
        image = r5g6b5ToRGB(texture_info.height, texture_info.width, pixelDump);
      } else if (texture_info.format == D3DFMT_A8R8G8B8) {
        image = a8r8g8b8ToRBG(texture_info.height, texture_info.width, pixelDump);
      } else if (texture_info.format == D3DFMT_DXT3){
        //
      }

      /*
      if (is_luminance_format(format)) {
         return patch_luminance_format(std::move(image), format);
      }
      */

      delete[] pixelDump;

      return image;
   }

   throw Badformat_exception{"bad format"};
}

auto read_texture(Ucfb_reader_strict<"tex_"_mn> texture)
   -> std::pair<std::string, cv::Mat>
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

   throw std::runtime_error{fmt::format("Texture {} has no usable formats!", name)};
}
}

void handle_texture(Ucfb_reader texture, File_saver& file_saver, Image_format save_format,
                    Model_format model_format)
{
   auto [name, image] = read_texture(Ucfb_reader_strict<"tex_"_mn>{texture});

   save_image(name, std::move(image), file_saver, save_format, model_format);
}
