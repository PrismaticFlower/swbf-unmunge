#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

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

class Swbf_fnv_hashes {
public:
   Swbf_fnv_hashes() = default;

   Swbf_fnv_hashes(const Swbf_fnv_hashes&) = delete;
   auto operator=(const Swbf_fnv_hashes&) -> Swbf_fnv_hashes& = delete;

   Swbf_fnv_hashes(Swbf_fnv_hashes&&) = default;
   auto operator=(Swbf_fnv_hashes&& other) -> Swbf_fnv_hashes& = default;

   auto lookup(const std::uint32_t hash) const noexcept -> std::string;

   void add(std::string string) noexcept;

private:
   std::unordered_map<std::uint32_t, std::string> _extra_hashes;
};

void read_swbf_fnv_hash_dictionary(Swbf_fnv_hashes& swbf_fnv_hashes,
                                   const std::filesystem::path& path);
