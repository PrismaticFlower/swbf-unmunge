#pragma once

#include <type_traits>

template<typename Flags_type>
constexpr bool are_flags_set(const Flags_type value, const Flags_type flags)
{
   using T = std::underlying_type_t<Flags_type>;

   return (static_cast<T>(value) & static_cast<T>(flags)) == static_cast<T>(flags);
}

template<typename Flags_type>
constexpr Flags_type set_flags(const Flags_type value, const Flags_type flags)
{
   using T = std::underlying_type_t<Flags_type>;

   const T result = static_cast<T>(value) | static_cast<T>(flags);

   return static_cast<Flags_type>(value);
}

template<typename Flags_type>
constexpr Flags_type clear_flags(const Flags_type value, const Flags_type flags)
{
   using T = std::underlying_type_t<Flags_type>;

   const T result = static_cast<T>(value) & ~(static_cast<T>(flags));

   return static_cast<Flags_type>(value);
}

template<typename Flags_type>
constexpr Flags_type toggle_flags(const Flags_type value, const Flags_type flags)
{
   using T = std::underlying_type_t<Flags_type>;

   const T result = static_cast<T>(value) ^ static_cast<T>(flags);

   return static_cast<Flags_type>(value);
}