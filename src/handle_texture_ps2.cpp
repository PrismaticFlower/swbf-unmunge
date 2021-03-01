

#include "DDS.h"
#include "app_options.hpp"
#include "file_saver.hpp"
#include "save_image.hpp"
#include "synced_cout.hpp"
#include "ucfb_reader.hpp"

#include <DirectXTex.h>

#include <gsl/gsl>

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace std::literals;

namespace {

enum class Ps2_format : std::uint16_t { t_4bit = 4, t_8bit = 8, t_32bit = 32 };

struct Texture_info {
   std::uint16_t width;
   std::uint16_t height;
   Ps2_format format;
   std::uint16_t detail_compressed;
   float mip_distance;
   std::uint16_t mip_count;
};

constexpr bool is_palettized_format(const Ps2_format format)
{
   switch (format) {
   case Ps2_format::t_4bit:
   case Ps2_format::t_8bit:
      return true;
   }

   return false;
}

template<typename Entry_type>
auto decompress_body(Ucfb_reader_strict<"BODY"_mn> body, std::uint32_t expanded_size)
   -> std::vector<Entry_type>
{
   std::vector<Entry_type> decompressed;
   decompressed.reserve(expanded_size);

   while (body) {
      const auto seq_desc = body.read_trivial_unaligned<std::uint8_t>();
      const std::uint32_t seq_count = seq_desc & 0x7fu;
      const bool seq_duplicate = seq_desc >> 7u;

      if (seq_duplicate) {
         const auto entry = body.read_trivial_unaligned<Entry_type>();

         for (auto i = 0u; i <= seq_count; ++i) {
            decompressed.emplace_back(entry);
         }
      }
      else {
         for (auto i = 0u; i <= seq_count; ++i) {
            const auto entry = body.read_trivial_unaligned<Entry_type>();

            decompressed.emplace_back(entry);
         }
      }
   }

   decompressed.resize(expanded_size);

   return decompressed;
}

auto promote_4bit_refs(const std::vector<std::uint8_t>& palette_refs)
   -> std::vector<std::uint8_t>
{
   std::vector<std::uint8_t> promoted;
   promoted.reserve(palette_refs.size() * 2);

   for (const auto ref : palette_refs) {
      promoted.emplace_back(static_cast<std::uint8_t>(ref >> 4));
      promoted.emplace_back(static_cast<std::uint8_t>(ref & 0x0f));
   }

   return promoted;
}

auto resolve_palette_refs(const std::vector<std::uint32_t>& palette,
                          const std::vector<std::uint8_t>& palette_refs)
   -> std::vector<std::uint32_t>
{
   std::vector<std::uint32_t> resolved;
   resolved.reserve(palette_refs.size());

   for (const auto ref : palette_refs) {
      resolved.emplace_back(palette[ref]);
   }

   return resolved;
}

auto resolve_texels(const std::vector<std::uint32_t>& texels, Texture_info info)
   -> DirectX::ScratchImage
{
   DirectX::ScratchImage scratch_image;
   scratch_image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, info.width, info.height, 1, 1);

   auto image = gsl::make_span(scratch_image.GetPixels(), scratch_image.GetPixelsSize());

   for (decltype(texels.size()) i = 0; i < texels.size(); ++i) {
      image[i * 4u] = (texels[i] >> 24) & 0xff;
      image[i * 4u + 1u] = (texels[i] >> 16) & 0xff;
      image[i * 4u + 2u] = (texels[i] >> 8) & 0xff;
      image[i * 4u + 3u] = texels[i] & 0xff;
   }

   return scratch_image;
}

auto resolve_detail_compression(DirectX::ScratchImage colour_image,
                                DirectX::ScratchImage detail_image)
   -> DirectX::ScratchImage
{
   DirectX::ScratchImage resized;

   const auto result = DirectX::Resize(
      *colour_image.GetImage(0, 0, 0), detail_image.GetMetadata().width,
      detail_image.GetMetadata().height, DirectX::TEX_FILTER_DEFAULT, resized);

   if (!SUCCEEDED(result)) {
      synced_cout::print("Warning: Failed to resize colour texture in order to resolve "
                         "detail compression.\n");

      return colour_image;
   }

   auto colour_texels = gsl::make_span(resized.GetPixels(), resized.GetPixelsSize());
   auto detail_texels =
      gsl::make_span(detail_image.GetPixels(), detail_image.GetPixelsSize());

   const auto texel_count =
      detail_image.GetMetadata().width * detail_image.GetMetadata().height;

   for (auto i = 0u; i < texel_count; ++i) {
      const float detail = detail_texels[i * 4u + 3u] / 255.f;

      for (auto c = 0u; c < 3u; ++c) {
         const float colour = colour_texels[i * 4u + c] / 255.f;
         const auto resolved = (colour * detail) * 2.f;

         colour_texels[i * 4u + c] = static_cast<std::uint8_t>(resolved * 255.f);
      }
   }

   return resized;
}

auto read_texture_info(Ucfb_reader_strict<"INFO"_mn> info) -> Texture_info
{
   Texture_info texture_info;

   texture_info.width = info.read_trivial_unaligned<std::uint16_t>();
   texture_info.height = info.read_trivial_unaligned<std::uint16_t>();
   texture_info.format = info.read_trivial_unaligned<Ps2_format>();
   texture_info.detail_compressed = info.read_trivial_unaligned<std::uint16_t>();
   texture_info.mip_distance = info.read_trivial_unaligned<float>();
   texture_info.mip_count = info.read_trivial_unaligned<std::uint16_t>();

   return texture_info;
}

auto read_palette(Ucfb_reader_strict<"pal_"_mn> pal) -> std::vector<std::uint32_t>
{
   auto info = pal.read_child_strict<"INFO"_mn>();

   const auto entries = info.read_trivial_unaligned<std::uint16_t>();
   const auto unknown = info.read_trivial_unaligned<std::uint16_t>();

   if (unknown != 32) {
      synced_cout::print("Warning: Potentially unknown palette type encountered.");
   }

   auto body = pal.read_child_strict<"BODY"_mn>();

   return decompress_body<std::uint32_t>(body, entries);
}

auto read_texture(Ucfb_reader_strict<"tex_"_mn> tex, Ucfb_reader parent_reader)
   -> std::pair<std::string_view, DirectX::ScratchImage>
{
   const auto name = tex.read_child_strict<"NAME"_mn>().read_string();
   const auto info = read_texture_info(tex.read_child_strict<"INFO"_mn>());

   std::vector<std::uint32_t> texels;

   if (is_palettized_format(info.format)) {
      const auto palette = read_palette(tex.read_child_strict<"pal_"_mn>());

      std::vector<std::uint8_t> palette_refs;

      if (info.format == Ps2_format::t_4bit) {
         palette_refs = decompress_body<std::uint8_t>(tex.read_child_strict<"BODY"_mn>(),
                                                      info.width * info.height / 2);

         palette_refs = promote_4bit_refs(palette_refs);
      }
      else {
         palette_refs = decompress_body<std::uint8_t>(tex.read_child_strict<"BODY"_mn>(),
                                                      info.width * info.height);
      }

      texels = resolve_palette_refs(palette, palette_refs);
   }
   else {
      texels = decompress_body<std::uint32_t>(tex.read_child_strict<"BODY"_mn>(),
                                              info.width * info.height);
   }

   auto image = resolve_texels(texels, info);

   if (info.detail_compressed) {
      auto detail_tex = parent_reader.read_child_strict_optional<"tex_"_mn>();

      bool success = false;

      if (detail_tex) {
         auto [detail_name, detail_image] = read_texture(*detail_tex, parent_reader);

         if (detail_name.substr(detail_name.size() - 4, 4) == "_dtl"sv) {
            image = resolve_detail_compression(std::move(image), std::move(detail_image));

            success = true;
         }
      }

      if (!success) {
         synced_cout::print("Warning: Failed to read detail texture.\n   texture:",
                            name.data());
      }
   }

   return {name, std::move(image)};
}
}

void handle_texture_ps2(Ucfb_reader texture, Ucfb_reader parent_reader,
                        File_saver& file_saver, Image_format save_format,
                        Model_format model_format)
{
   auto [name, image] =
      read_texture(Ucfb_reader_strict<"tex_"_mn>{texture}, parent_reader);

   save_image(name, std::move(image), file_saver, save_format, model_format);
}
