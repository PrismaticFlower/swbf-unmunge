
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

#include <D3D9TypesMinimal.h>

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

/*
struct Image {
  int width, height;
  uint8_t *data; //static thread local, don't delete 
  D3DFORMAT format;

  Image(int w, int h, uint8_t *d, D3DFORMAT f) 
        : width(w), height(h), data(d), format(f);
};
*/

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

//From Ben1138's libSWBF2
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


auto read_format_list(Ucfb_reader_strict<"INFO"_mn> info) -> std::vector<D3DFORMAT>
{
   const auto count = info.read_trivial<std::uint32_t>();

   return info.read_array<D3DFORMAT>(count);
}


auto read_texture_format(Ucfb_reader_strict<"tex_"_mn> texture, const D3DFORMAT format, int& w, int& h)
   -> uint32_t *
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

      thread_local static uint8_t *sourceData = new uint8_t[1024 * 1024 * 4]; //max SWBF2 tex size
      thread_local static uint32_t *rgbaData = new uint32_t[1024 * 1024];

      auto body = lvl.read_child_strict<"BODY"_mn>();
      body.read_array_to_span(body.size(),
         gsl::make_span(sourceData, body.size()));

      w = texture_info.width;
      h = texture_info.height;
      
      COUT(fmt::format("Width {}, Height {}, Numbytes {}, Format {}",
                      w, h, body.size(), 
                      D3DToString(texture_info.format)))
      
      switch (texture_info.format){
        case D3DFMT_R5G6B5:
          r5g6b5ToRGBA(w, h, sourceData, rgbaData);
          break;
        case D3DFMT_A8R8G8B8:
          a8r8g8b8ToRBGA(w, h, sourceData, rgbaData);
          break;
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
          bc2ToRGBA(w, h, sourceData, rgbaData);
          break;
        case D3DFMT_L8:
        case D3DFMT_A8L8:
        case D3DFMT_L16:
        case D3DFMT_A4L4:
          lumToRGBA(w, h, sourceData, rgbaData, texture_info.format);
          break;
        default:
          w = h = 0;
      }

      return rgbaData;
   }

   throw Badformat_exception{"bad format"};
}

auto read_texture(Ucfb_reader_strict<"tex_"_mn> texture, int& w, int& h)
   -> std::pair<std::string, uint32_t *>
{
   const auto name = texture.read_child_strict<"NAME"_mn>().read_string();

   const auto formats =
      sort_formats(read_format_list(texture.read_child_strict<"INFO"_mn>()));

   for (const auto format : formats) {
      try {
        COUT(std::string{name})
         return {std::string{name}, read_texture_format(texture, format, w, h)};
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
   int w,h;
   auto [name, imageData] = read_texture(Ucfb_reader_strict<"tex_"_mn>{texture}, w, h);

   //Should probably write an image wrapper struct...
   if (w * h > 0)
    save_image(name, imageData, file_saver, save_format, model_format, w, h);
}
