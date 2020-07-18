
#include "app_options.hpp"
#include "file_saver.hpp"

#include "string_helpers.hpp"
#include "save_image_unix.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stbi/stb_image_write.h"

#include <exception>
#include <string_view>
#include <string>

using namespace std::literals;

namespace {


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

void save_image(std::string_view name, uint32_t *data,
                File_saver& file_saver, Image_format save_format,
                Model_format model_format, int w, int h)
{
   // Windows' 3D Viewer doesn't handle relative texture paths, so we have to put the
   // textures in the same folder as the glTF files if we want them to be previewable in
   // it.
   const auto dir = model_format == Model_format::gltf2 ? "models"sv : "textures"sv;

   // glTF doesn't support .tga files.
   save_format = model_format == Model_format::gltf2 ? Image_format::png : save_format;
  
   const auto path = file_saver.build_file_path(dir, name, image_extension(save_format));

   file_saver.create_dir(dir);

   if (save_format == Image_format::tga){
      stbi_write_tga(path.string().c_str(), w, h, 4, reinterpret_cast<void *>(data));
   } else {
      stbi_write_png(path.string().c_str(), w, h, 4, reinterpret_cast<void *>(data), w*4);
   }
}