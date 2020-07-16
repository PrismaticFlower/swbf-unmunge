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

   return static_cast<Flags_type>(result);
}

template<typename Flags_type>
constexpr Flags_type clear_flags(const Flags_type value, const Flags_type flags)
{
   using T = std::underlying_type_t<Flags_type>;

   const T result = static_cast<T>(value) & ~(static_cast<T>(flags));

   return static_cast<Flags_type>(result);
}

template<typename Flags_type>
constexpr Flags_type toggle_flags(const Flags_type value, const Flags_type flags)
{
   using T = std::underlying_type_t<Flags_type>;

   const T result = static_cast<T>(value) ^ static_cast<T>(flags);

   return static_cast<Flags_type>(result);
}

template<typename Type>
constexpr bool marked_as_enum_flag(Type&&) noexcept
{
   return false;
}

template<typename Enum>
struct is_enum_flag : std::bool_constant<marked_as_enum_flag(Enum{})> {
};

template<typename Enum>
constexpr bool is_enum_flag_v = is_enum_flag<Enum>::value;

template<typename Enum, typename = std::enable_if_t<is_enum_flag_v<Enum>>>
constexpr Enum operator|(const Enum l, const Enum r) noexcept
{
   return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(l) |
                            static_cast<std::underlying_type_t<Enum>>(r));
}

template<typename Enum, typename = std::enable_if_t<is_enum_flag_v<Enum>>>
constexpr Enum operator&(Enum l, Enum r) noexcept
{
   return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(l) &
                            static_cast<std::underlying_type_t<Enum>>(r));
}

template<typename Enum, typename = std::enable_if_t<is_enum_flag_v<Enum>>>
constexpr Enum operator^(const Enum l, const Enum r) noexcept
{
   return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(l) ^
                            static_cast<std::underlying_type_t<Enum>>(r));
}

template<typename Enum, typename = std::enable_if_t<is_enum_flag_v<Enum>>>
constexpr Enum operator~(const Enum f) noexcept
{
   return static_cast<Enum>(~static_cast<std::underlying_type_t<Enum>>(f));
}

template<typename Enum, typename = std::enable_if_t<is_enum_flag_v<Enum>>>
constexpr Enum& operator|=(Enum& l, const Enum r) noexcept
{
   return l = l | r;
}

template<typename Enum, typename = std::enable_if_t<is_enum_flag_v<Enum>>>
constexpr Enum& operator&=(Enum& l, const Enum r) noexcept
{
   return l = l & r;
}

template<typename Enum, typename = std::enable_if_t<is_enum_flag_v<Enum>>>
constexpr Enum& operator^=(Enum& l, const Enum r) noexcept
{
   return l = l ^ r;
}
