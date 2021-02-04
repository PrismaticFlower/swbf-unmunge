#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cstring>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

template<typename Char_type, typename Function,
         typename Char_traits = std::char_traits<Char_type>>
inline void for_each_substr(
   typename std::common_type<std::basic_string_view<Char_type, Char_traits>>::type string,
   const Char_type delimiter, Function function)
{
   for (auto offset = string.find(delimiter); (offset != string.npos);
        offset = string.find(delimiter)) {
      function(string.substr(0, offset));

      string.remove_prefix(offset + 1);
   }

   if (!string.empty()) function(string);
}

template<typename Char_type, typename Char_traits = std::char_traits<Char_type>>
constexpr auto split_string(
   typename std::common_type<std::basic_string_view<Char_type, Char_traits>>::type string,
   const Char_type delimiter) noexcept
   -> std::array<std::basic_string_view<Char_type, Char_traits>, 2>
{
   const auto offset = string.find(delimiter);

   if (offset == string.npos) return {string, decltype(string){}};

   const auto other = string.substr(0, offset);
   string.remove_prefix(offset + 1);

   return {other, string};
}

template<typename Char_t, typename Char_triats = std::char_traits<Char_t>>
constexpr bool begins_with(
   std::basic_string_view<Char_t, Char_triats> string,
   typename std::common_type<std::basic_string_view<Char_t, Char_triats>>::type
      what) noexcept
{
   if (what.size() > string.size()) return false;

   return (string.substr(0, what.size()) == what);
}

template<typename Char_t, typename Char_triats = std::char_traits<Char_t>>
constexpr bool begins_with(
   const std::basic_string<Char_t, Char_triats>& string,
   typename std::common_type<std::basic_string_view<Char_t, Char_triats>>::type
      what) noexcept
{
   return begins_with(std::basic_string_view<Char_t, Char_triats>{string}, what);
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

inline auto to_hexstring(const std::int64_t integer) noexcept -> std::string
{
   std::array<char, 18> chars{'0', 'x'};

   auto [last, er] = std::to_chars(&chars[2], &chars.back(), integer, 16);

   assert(er == std::errc{});

   return {chars.data(), last};
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
