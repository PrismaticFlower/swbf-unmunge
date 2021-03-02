#include "ucfb_reader.hpp"
#include "app_options.hpp"
#include "swbf_fnv_hashes.hpp"

#include <stdexcept>

Ucfb_reader::Ucfb_reader(const gsl::span<const std::byte> bytes)
{
   if (bytes.size() < 8) {
      throw std::runtime_error{"Size of data is too small."};
   }

   _mn = reinterpret_span_as<Magic_number>(
      gsl::span<const std::byte, sizeof(Magic_number)>{&bytes[0], sizeof(Magic_number)});

   _size = reinterpret_span_as<std::uint32_t>(
      gsl::span<const std::byte, sizeof(std::uint32_t)>{&bytes[4],
                                                        sizeof(std::uint32_t)});
   _data = bytes.data() + 8;

   if (_size > static_cast<std::size_t>(bytes.size() - 8)) {
      throw std::runtime_error{
         "Size of supplied memory is less than size of supposed chunk."};
   }
}

Ucfb_reader::Ucfb_reader(const Magic_number mn, const std::uint32_t size,
                         const std::byte* const data)
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

auto Ucfb_reader::read_child(const std::nothrow_t, const bool unaligned) noexcept
   -> std::optional<Ucfb_reader>
{
   if ((_head + 8) > _size) return std::nullopt;

   const auto old_head = _head;

   const auto child_mn = read_trivial<Magic_number>();
   const auto child_size = read_trivial<std::uint32_t>();
   const auto child_data_offset = _head;

   _head += child_size;

   if (_head > _size) {
      _head = old_head;

      return std::nullopt;
   }

   if (!unaligned) align_head();

   return Ucfb_reader{child_mn, child_size, _data + child_data_offset};
}

Ucfb_reader Ucfb_reader::read_child_strict(const Magic_number child_mn,
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

auto Ucfb_reader::read_child_strict_optional(const Magic_number child_mn,
                                             const bool unaligned)
   -> std::optional<Ucfb_reader>
{
   const auto old_head = _head;

   const auto child = read_child(unaligned);

   if (child.magic_number() != child_mn) {
      _head = old_head;

      return {};
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

auto Ucfb_reader::read_string(const bool unaligned) -> std::string_view
{
   const char* const string = to_char_pointer(_data + _head);
   const auto string_length = cstring_length(string, _size - _head);

   _head += (string_length + 1);

   check_head();

   if (!unaligned) align_head();

   if (get_pre_processing_global()) add_found_string(string);

   return {string, string_length};
}
