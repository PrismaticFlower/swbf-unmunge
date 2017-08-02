#pragma once

#include "string_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

enum class Magic_number : std::uint32_t {};

constexpr Magic_number create_magic_number(const char c_0, const char c_1, const char c_2,
                                           const char c_3)
{
   std::uint32_t result = 0;

   result |= (static_cast<std::uint8_t>(c_0) << 0);
   result |= (static_cast<std::uint8_t>(c_1) << 8);
   result |= (static_cast<std::uint8_t>(c_2) << 16);
   result |= (static_cast<std::uint8_t>(c_3) << 24);

   return static_cast<Magic_number>(result);
}

constexpr Magic_number create_magic_number(const std::array<char, 4> chars)
{
   return create_magic_number(chars[0], chars[1], chars[2], chars[3]);
}

constexpr Magic_number operator""_mn(const char* chars, const std::size_t) noexcept
{
   return create_magic_number(chars[0], chars[1], chars[2], chars[3]);
}

inline std::string serialize_magic_number(const Magic_number magic_number)
{
   const auto number = static_cast<std::uint32_t>(magic_number);

   std::stringstream serialized;

   serialized << std::hex;

   serialized << ((number >> 0u) & 0xFFu) << '-' << ((number >> 8u) & 0xFFu) << '-'
              << ((number >> 16u) & 0xFFu) << '-' << ((number >> 24u) & 0xFFu);

   return serialized.str();
}

inline Magic_number deserialize_magic_number(std::string_view serialized) noexcept
{
   std::array<char, 4> chars{};
   std::array<char, 4>::size_type i = 0;

   const auto body = [&chars, &i](auto string) {
      if (i > chars.size()) return;

      chars[i] = static_cast<char>(std::stol(std::string{string}, nullptr, 16));

      ++i;
   };

   for_each_substr(serialized, '-', body);

   return create_magic_number(chars);
}
