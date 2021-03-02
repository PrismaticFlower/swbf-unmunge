#pragma once

#include <cstdint>
#include <string>

constexpr std::uint32_t fnv_1a_hash(const std::string_view str)
{
   constexpr std::uint32_t FNV_prime = 16777619;
   constexpr std::uint32_t offset_basis = 2166136261;

   std::uint32_t hash = offset_basis;

   for (auto c : str) {
      c |= 0x20;

      hash ^= c;
      hash *= FNV_prime;
   }

   return hash;
}

constexpr std::uint32_t operator""_fnv(const char* str, const std::size_t length)
{
   return fnv_1a_hash({str, length});
}

std::string lookup_fnv_hash(std::uint32_t hash);

void read_fnv_dictionary(std::string file_name);

void write_fnv_dictionary(std::string file_name);

bool add_fnv_hash(std::string str);

bool add_found_string(std::string str);
