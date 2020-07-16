
#include "save_image_tga.hpp"
#include "type_pun.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace {

constexpr int rgba_image_type = 2;

struct Tga_header {
   std::uint8_t image_id_length = 0;
   std::uint8_t color_map_type = 0;
   std::uint8_t image_type = rgba_image_type;
   std::uint8_t color_map_index[2]{};
   std::uint8_t color_map_length[2]{};
   std::uint8_t color_map_entry_size{};
   std::uint16_t image_x_origin{};
   std::uint16_t image_y_origin{};
   std::uint16_t image_width;
   std::uint16_t image_height;
   std::uint8_t image_pixel_depth = 32;
   std::uint8_t image_description{};
};

static_assert(sizeof(Tga_header) == 18);

bool supports_format(const DXGI_FORMAT format)
{
   switch (format) {
   case DXGI_FORMAT_R8G8B8A8_TYPELESS:
   case DXGI_FORMAT_B8G8R8A8_TYPELESS:
   case DXGI_FORMAT_B8G8R8X8_TYPELESS:
      return true;
   }

   return false;
}

}

void save_image_tga(const std::filesystem::path& save_path, DirectX::Image image)
{
   const auto typeless_format = DirectX::MakeTypeless(image.format);

   if (!supports_format(typeless_format)) {
      throw std::runtime_error{"Invalid image format passed to TGA save function!"};
   }

   std::ofstream out{save_path, std::ios::binary};

   Tga_header header{};

   header.image_width = static_cast<std::uint16_t>(image.width);
   header.image_height = static_cast<std::uint16_t>(image.height);

   out.write(to_char_pointer(&header), sizeof(header));

   const auto height = static_cast<std::ptrdiff_t>(image.height);
   const auto width = static_cast<std::ptrdiff_t>(image.width);

   for (std::ptrdiff_t y = height - 1; y >= 0; --y) {
      for (std::ptrdiff_t x = 0; x < width; ++x) {
         std::array<std::uint8_t, 4> rgba;

         std::memcpy(&rgba,
                     image.pixels + (y * image.rowPitch) + (x * sizeof(std::uint32_t)),
                     sizeof(rgba));

         if (typeless_format == DXGI_FORMAT_R8G8B8A8_TYPELESS) {
            std::swap(rgba[0], rgba[2]);
         }
         else if (typeless_format == DXGI_FORMAT_B8G8R8X8_TYPELESS) {
            rgba[3] = 0xff;
         }

         out.write(to_char_pointer(rgba.data()), sizeof(rgba));
      }
   }
}
