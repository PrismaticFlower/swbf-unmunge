#include "ucfb_builder.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
std::size_t needed_padding(const std::size_t size) noexcept
{
   if (size % 4) {
      return 4 - (size % 4);
   }

   return 0;
}
}

Ucfb_builder::Ucfb_builder(Magic_number magic_number)
{
   _magic_number = magic_number;
}

Ucfb_builder::Ucfb_builder(const fs::path& file_path, Magic_number magic_number)
   : Ucfb_builder{magic_number}
{
   const auto size = fs::file_size(file_path);

   std::ifstream file{file_path, std::ios::in | std::ios::binary};

   _contents.resize(size);

   file.read(_contents.data(), size);
}

Magic_number Ucfb_builder::get_magic_number() const noexcept
{
   return _magic_number;
}

void Ucfb_builder::add_child(const Ucfb_builder& child)
{
   _children.push_back(child);
}

void Ucfb_builder::add_child(Ucfb_builder&& child)
{
   _children.emplace_back(std::move(child));
}

Ucfb_builder& Ucfb_builder::emplace_child(Magic_number magic_number)
{
   _children.emplace_back(magic_number);

   return _children.back();
}

void Ucfb_builder::write(std::string_view str, bool null_terminate, bool aligned)
{
   _contents += str;

   if (null_terminate) _contents += '\0';

   if (aligned) pad_till_aligned();
}

void Ucfb_builder::pad_till_aligned()
{
   _contents.append(needed_padding(_contents.size()), '\0');
}

std::string Ucfb_builder::create_buffer() const
{
   const auto size = calc_size();

   if (size > std::numeric_limits<std::uint32_t>::max()) {
      throw std::out_of_range{"ucfb file too large"};
   }

   std::string buffer;
   buffer.reserve(size);

   buffer += view_pod_as_string(_magic_number);

   const auto size_minus_header = static_cast<std::uint32_t>(size - 8);
   buffer += view_pod_as_string(size_minus_header);

   buffer += _contents;
   buffer.append(needed_padding(buffer.size()), '\0');

   for (const auto& child : _children) {
      buffer += child.create_buffer();
      buffer.append(needed_padding(buffer.size()), '\0');
   }

   return buffer;
}

std::size_t Ucfb_builder::calc_size() const noexcept
{
   std::size_t size = 8 + _contents.size();

   for (const auto& child : _children) {
      size += child.calc_size();
      size += needed_padding(size);
   }

   return size;
}
