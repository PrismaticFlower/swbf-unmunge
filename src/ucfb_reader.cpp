#include "ucfb_reader.hpp"

#include <stdexcept>

Ucfb_reader::Ucfb_reader(const Byte* bytes, const std::uint32_t size)
   : _mn{view_type_as<Magic_number>(bytes[0])},
     _size{view_type_as<std::uint32_t>(bytes[4])}, _data{bytes + 8}
{
   Expects((size >= 8));

   if (_size > (size - 8)) {
      throw std::runtime_error{
         "Size of supplied memory is less than size of supposed chunk."};
   }
}

Ucfb_reader::Ucfb_reader(const Magic_number mn, const std::uint32_t size,
                         const Byte* const data)
   : _mn{mn}, _size{size}, _data{data}
{
}

Ucfb_reader Ucfb_reader::read_child(const bool unaligned)
{
   const auto child_mn = read_trivial<Magic_number>();
   const auto child_size = read_trivial<std::uint32_t>();
   const auto child_data_offset = _head;

   _head += child_size;

   check_head();

   if (!unaligned) align_head();

   return Ucfb_reader{child_mn, child_size, _data + child_data_offset};
}

Ucfb_reader Ucfb_reader::read_child_strict_impl(const Magic_number child_mn,
                                                const bool unaligned)
{
   const auto old_head = _head;

   const auto child = read_child(unaligned);

   if (child.magic_number() != child_mn) {
      _head = old_head;

      throw std::runtime_error{"Chunk magic number mistmatch"
                               " when performing strict read of child chunk."};
   }

   return child;
}

void Ucfb_reader::consume(const std::size_t amount, const bool unaligned)
{
   _head += amount;

   check_head();

   if (!unaligned) align_head();
}

Ucfb_reader::operator bool() const noexcept
{
   return (_head < _size);
}

void Ucfb_reader::reset_head() noexcept
{
   _head = 0;
}

Magic_number Ucfb_reader::magic_number() const noexcept
{
   return _mn;
}

std::size_t Ucfb_reader::size() const noexcept
{
   return _size;
}

void Ucfb_reader::check_head()
{
   if (_head > _size) {
      throw std::runtime_error{"Attempt to read past end of chunk."};
   }
}
