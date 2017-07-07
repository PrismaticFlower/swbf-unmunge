#pragma once

#include<cstring>
#include<string>
#include<utility>

constexpr std::string_view operator""_sv(const char* str, std::size_t size)
{
   return {str, size};
}

constexpr std::u16string_view operator""_sv(const char16_t* str, std::size_t size)
{
   return {str, size};
}

constexpr std::u32string_view operator""_sv(const char32_t* str, std::size_t size)
{
   return {str, size};
}

constexpr std::wstring_view operator""_sv(const wchar_t* str, std::size_t size)
{
   return {str, size};
}

inline bool string_is_number(std::string_view string) noexcept
{
   const auto is_char_digit = [] (const char& c)
   {
      return (c >= '0' && c <= '9');
   };

   const auto is_char_control = [](const char& c)
   {
      return (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-');
   };

   for (const auto& c : string) {

      if (!is_char_digit(c) && !is_char_control(c)) return false;
   }

   return true;
}

inline std::string to_hexstring(std::uint32_t integer) noexcept
{
   const auto get_byte = []
   (std::uint32_t integer, std::uint32_t index) noexcept -> std::uint8_t
   {
      return (integer >> (index * 8)) & 0xFF;;
   };

   const std::uint8_t bytes[4] = {
      get_byte(integer, 3), 
      get_byte(integer, 2),
      get_byte(integer, 1),
      get_byte(integer, 0)};

   std::string result{"0x"_sv};
   result.reserve(10);

   const char table[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

   const auto split_byte = []
   (std::uint8_t byte) noexcept
   {
      std::uint8_t first = (byte >> 4) & 0x0F;
      std::uint8_t second = byte & 0x0F;

      return std::make_pair(first, second);
   };

   for (const auto& byte : bytes) {
      const auto byte_pair = split_byte(byte);

      result.push_back(table[byte_pair.first]);
      result.push_back(table[byte_pair.second]);
   }

   return result;
}

inline void copy_to_cstring(std::string_view from, char*const to, const std::size_t size)
{
   const std::size_t length = (from.length() > size - 1) ? (size - 1) : from.length();

   std::memcpy(to, from.data(), length);
   to[length] = '\0';
}
