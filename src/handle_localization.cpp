
#include "chunk_handlers.hpp"
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"

#include "tbb/task_group.h"

using namespace std::literals;

namespace {

struct Entry {
   std::uint32_t hash;
   std::uint16_t next_offset;
   char16_t string[];
};

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

void dump_localization(const chunks::Localization& locl, File_saver& file_saver)
{
   std::string name{reinterpret_cast<const char*>(&locl.bytes[0]), locl.name_size - 1};
   name += ".txt"_sv;

   std::uint32_t head = locl.name_size;
   const std::uint32_t end = locl.size - sizeof(chunks::Localization);

   const auto align_head = [&head] {
      if (head % 4 != 0) head += (4 - (head % 4));
   };

   align_head();

   // Consume the Body header.
   head += 8;

   std::string buffer;
   buffer.reserve(16384);

   while (head < end) {
      const Entry& entry = view_type_as<Entry>(locl.bytes[head]);

      std::u16string_view string{entry.string};

      buffer += to_hexstring(entry.hash);
      buffer += ' ';
      buffer += cast_encoding(string);
      buffer += '\n';

      head += entry.next_offset;
   }

   file_saver.save_file(std::move(buffer), std::move(name), "localization"s);
}
}

void handle_localization(const chunks::Localization& locl, tbb::task_group& tasks,
                         File_saver& file_saver)
{
   tasks.run([&locl, &file_saver] {
      std::string name{reinterpret_cast<const char*>(&locl.bytes[0]), locl.name_size - 1};
      name += ".loc"_sv;

      handle_unknown(view_type_as<chunks::Unknown>(locl), file_saver, std::move(name));
   });

   tasks.run([&locl, &file_saver] { dump_localization(locl, file_saver); });
}