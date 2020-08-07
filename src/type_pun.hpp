#pragma once

#include <gsl/gsl>

#include <cstddef>
#include <cstring>
#include <string>


#ifndef _WIN32
constexpr int16_t operator "" _i16(unsigned long long int lit){return ((int16_t) lit);}
constexpr int32_t operator "" _i32(unsigned long long int lit){return ((int32_t) lit);}

constexpr uint16_t operator "" _ui16(unsigned long long int lit){return ((uint16_t) lit);}
constexpr uint32_t operator "" _ui32(unsigned long long int lit){return ((uint32_t) lit);}
#endif

template<typename Type, std::ptrdiff_t extent>
inline auto reinterpret_span_as(const gsl::span<const std::byte, extent> data) noexcept
   -> Type
{
   static_assert(std::is_trivially_copyable_v<Type>, "Type must be trivially copyable!");
   static_assert(sizeof(Type) == extent, "Type and span must be the same size!");
   static_assert(std::is_default_constructible_v<Type>,
                 "Type must be default constructible!");

   Type type;

   std::memcpy(&type, data.data(), sizeof(Type));

   return type;
}


//Fixes Clang errors
#ifndef _WIN32

template<typename Type, std::size_t extent>
inline auto reinterpret_span_as(const gsl::span<const std::byte, extent> data) noexcept
   -> Type
{
   static_assert(std::is_trivially_copyable_v<Type>, "Type must be trivially copyable!");
   static_assert(sizeof(Type) == extent, "Type and span must be the same size!");
   static_assert(std::is_default_constructible_v<Type>,
                 "Type must be default constructible!");

   Type type;

   std::memcpy(&type, data.data(), sizeof(Type));

   return type;
}

#endif


template<typename Type>
inline auto to_char_pointer(const Type* const pointer) noexcept -> const char*
{
   return reinterpret_cast<const char*>(pointer);
}

template<typename Type>
inline auto to_char_pointer(Type* const pointer) noexcept -> char*
{
   return reinterpret_cast<char*>(pointer);
}

template<typename Type>
inline auto to_byte_pointer(const Type* const pointer) noexcept -> const std::byte*
{
   return reinterpret_cast<const std::byte*>(pointer);
}

template<typename Type>
inline auto view_object_as_string(const Type& pod) noexcept -> std::string_view
{
   static_assert(std::is_trivially_copyable_v<std::remove_reference_t<Type>>,
                 "Type must be trivially copyable.");
   static_assert(!std::is_pointer_v<std::remove_reference_t<Type>>,
                 "Type can not be a pointer.");

   return {to_char_pointer(&pod), sizeof(Type)};
}

template<typename Type>
inline auto view_object_as_string(Type&& pod) noexcept -> std::string
{
   static_assert(std::is_trivially_copyable_v<std::remove_reference_t<Type>>,
                 "Type must be trivially copyable.");
   static_assert(!std::is_pointer_v<std::remove_reference_t<Type>>,
                 "Type can not be rvalue to pointer.");

   return {to_char_pointer(&pod), sizeof(Type)};
}

template<typename Type>
inline auto view_object_span_as_string(gsl::span<const Type> array) noexcept
   -> std::string_view
{
   static_assert(std::is_trivially_copyable_v<std::remove_reference_t<Type>>,
                 "Type must be trivially copyable.");
   static_assert(!std::is_pointer_v<std::remove_reference_t<Type>>,
                 "Type can not be a pointer.");

   return {to_char_pointer(array.data()), static_cast<std::size_t>(array.size_bytes())};
}
