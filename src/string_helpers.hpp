#pragma once

#include <algorithm>
#include <cstring>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

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
   const auto is_char_digit = [](const char& c) { return (c >= '0' && c <= '9'); };

   const auto is_char_control = [](const char& c) {
      return (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-');
   };

   for (const auto& c : string) {

      if (!is_char_digit(c) && !is_char_control(c)) return false;
   }

   return true;
}

template<typename Integral>
inline std::string to_hexstring(const Integral integer) noexcept
{
   static_assert(std::is_integral_v<Integral>,
                 "Function can only be used with integral types!");

   std::ostringstream stream;
   stream << std::hex << std::showbase << integer;

   return stream.str();
}

inline void copy_to_cstring(std::string_view from, char* const to, const std::size_t size)
{
   const std::size_t length = (from.length() > size - 1) ? (size - 1) : from.length();

   std::memcpy(to, from.data(), length);
   to[length] = '\0';
}

template<typename Char_type, typename Size_type>
inline std::size_t cstring_length(const Char_type* const string,
                                  const Size_type max_length)
{
   const auto string_end =
      std::find(string, string + max_length, static_cast<Char_type>('\0'));

   return static_cast<Size_type>(std::distance(string, string_end));
}