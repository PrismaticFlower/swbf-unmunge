
#include "chunk_handlers.hpp"
#include "file_saver.hpp"
#include "string_helpers.hpp"
#include "swbf_fnv_hashes.hpp"
#include "ucfb_reader.hpp"

#include "tbb/task_group.h"

using namespace std::literals;

namespace {

std::string cast_encoding(std::u16string_view from) noexcept
{
   const auto get_byte = [](std::uint32_t integer, std::uint32_t index) noexcept->char
   {
      return (integer >> (index * 8)) & 0xFF;
      ;
   };

   const auto get_bit_mask = [](auto length) noexcept
   {
      decltype(length) rt{};

      for (decltype(length) i = 0; i < length; ++i) {
         rt |= (1 << i);
      }

      return rt;
   };

   static_assert(CHAR_BIT == 8, "char must have eight bits.");

   std::string rt;

   rt.reserve(from.length() * 3);

   const auto add_char = [&rt](char c) { rt.push_back(c); };

   const auto add_invalid_char = [&rt] {
      rt.push_back('\xEF');
      rt.push_back('\xBF');
      rt.push_back('\xBD');
   };

   for (std::size_t i = 0; i < from.length(); ++i) {
      if (from[i] <= 0x7F) {
         add_char(static_cast<char>(from[i]));
      }
      else if (from[i] <= 0x7FF) {
         std::uint8_t low_byte = get_byte(from[i], 0);

         std::uint8_t high_byte = 0xC0;
         high_byte |= (get_byte(from[i], 1) << 2);

         high_byte |= ((low_byte >> 6) & get_bit_mask(2));

         low_byte = (low_byte & ~(0xC0)) | 0x80;

         add_char(static_cast<char>(high_byte));
         add_char(static_cast<char>(low_byte));
      }
      else if (from[i] >= 0xd800 && from[i] <= 0xdfff) {
         if (i + 1 >= from.length()) {
            add_invalid_char();

            continue;
         }

         const char16_t high_surrogate = from[i];
         const char16_t low_surrogate = from[++i];

         if (from[i] < 0xd800 || from[i] > 0xdfff) {
            add_invalid_char();

            continue;
         }

         const char32_t x =
            (high_surrogate & ((1 << 6) - 1)) << 10 | low_surrogate & ((1 << 10) - 1);
         const char32_t w = (high_surrogate >> 6) & ((1 << 5) - 1);
         const char32_t u = w + 1;

         const char32_t c = (u << 16 | x);

         std::uint8_t low_byte = get_byte(c, 0);
         std::uint8_t mid_low_byte = get_byte(c, 1);
         std::uint8_t mid_high_byte = (get_byte(c, 2) << 3);
         std::uint8_t high_byte = 0xF0;

         high_byte |= ((mid_high_byte >> 5) & get_bit_mask(3));

         mid_high_byte <<= 1;

         mid_high_byte |= ((mid_low_byte >> 4) & get_bit_mask(4));

         mid_low_byte <<= 2;

         mid_low_byte |= ((low_byte >> 6) & get_bit_mask(2));

         low_byte = (low_byte & ~(0xC0)) | 0x80;
         mid_low_byte = (mid_low_byte & ~(0xC0)) | 0x80;
         mid_high_byte = (mid_high_byte & ~(0xC0)) | 0x80;

         add_char(static_cast<char>(high_byte));
         add_char(static_cast<char>(mid_high_byte));
         add_char(static_cast<char>(mid_low_byte));
         add_char(static_cast<char>(low_byte));
      }
      else if (from[i] <= 0xFFFF) {
         if (from[i] == 0xfffe || from[i] == 0xffff) continue;

         std::uint8_t low_byte = get_byte(from[i], 0);
         std::uint8_t mid_byte = get_byte(from[i], 1);
         std::uint8_t high_byte = 0xE0;

         high_byte |= ((mid_byte >> 4) & get_bit_mask(4));

         mid_byte <<= 2;

         mid_byte |= ((low_byte >> 6) & get_bit_mask(2));

         mid_byte = (mid_byte & ~(0xC0)) | 0x80;
         low_byte = (low_byte & ~(0xC0)) | 0x80;

         add_char(static_cast<char>(high_byte));
         add_char(static_cast<char>(mid_byte));
         add_char(static_cast<char>(low_byte));
      }
   }

   return rt;
}

void dump_localization(Ucfb_reader_strict<"Locl"_mn> localization, File_saver& file_saver)
{
   const auto name = localization.read_child_strict<"NAME"_mn>().read_string();

   std::string buffer;
   buffer.reserve(16384);

   auto body = localization.read_child_strict<"BODY"_mn>();
   std::string tmp;
   int pos = -1;

   for (auto u16str_buf =
           [] {
              std::vector<char16_t> buf;
              buf.reserve(256);

              return buf;
           }();
        body;) {
      const auto hash = body.read_trivial<std::uint32_t>();

      if (hash == 0) break;

      const auto section_size = body.read_trivial_unaligned<std::uint16_t>();
      body.read_array<char16_t>((section_size - 6) / 2, u16str_buf);

      buffer += lookup_fnv_hash(hash);
      buffer += "=\"";
      tmp = cast_encoding({u16str_buf.data()});

	  // escape the backslash
	  pos = tmp.find("\\");
      while (pos > -1) {
         tmp.insert(pos, "\\");
         pos = tmp.find("\\", pos + 2);
      }

	  // escape the double quote
      pos = tmp.find("\"");
      while (pos > -1) {
         tmp.insert(pos, "\\");
         pos = tmp.find("\"", pos + 2);
      }

      buffer += tmp;
      buffer += "\"\n";
   }

   file_saver.save_file(buffer, "localization"sv, name, ".txt"sv);
}
}

void handle_localization(Ucfb_reader localization, File_saver& file_saver)
{
   tbb::task_group tasks;

   tasks.run([localization, &file_saver]() {
      auto localization_copy = localization;
      const auto name = localization_copy.read_child_strict<"NAME"_mn>().read_string();

      handle_unknown(localization, file_saver, name, ".loc"sv);
   });

   tasks.run_and_wait([localization, &file_saver] {
      dump_localization(Ucfb_reader_strict<"Locl"_mn>{localization}, file_saver);
   });
}
