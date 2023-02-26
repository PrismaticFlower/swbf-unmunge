#include "lua/lua4.hpp"

auto get_OP(const std::uint32_t i) -> std::uint32_t
{
   return i & MASK_OP;
}

auto get_U(const std::uint32_t i) -> std::uint32_t
{
   return (i & MASK_US) >> 6;
}

auto get_S(const std::uint32_t i) -> std::int32_t
{
   return (i & MASK_US) >> 6;
}

auto get_A(const std::uint32_t i) -> std::uint32_t
{
   return (i & MASK_A) >> 15;
}

auto get_B(const std::uint32_t i) -> std::uint32_t
{
   return (i & MASK_B) >> 6;
}