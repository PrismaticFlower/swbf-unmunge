#pragma once

#include "byte.hpp"
#include "chunk_headers.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"

#include <gsl/gsl>

#include <cstddef>
#include <string>
#include <type_traits>

template<Magic_number type_mn>
class Ucfb_reader_strict;

class Ucfb_reader {
public:
   Ucfb_reader() = delete;
   Ucfb_reader(const Byte* bytes, const std::uint32_t size);

   template<Magic_number type_mn>
   Ucfb_reader(const Ucfb_reader_strict<type_mn>&) = delete;
   template<Magic_number type_mn>
   Ucfb_reader& operator=(const Ucfb_reader_strict<type_mn>&) = delete;

   template<Magic_number type_mn>
   Ucfb_reader(Ucfb_reader_strict<type_mn>&&) = delete;
   template<Magic_number type_mn>
   Ucfb_reader& operator=(Ucfb_reader_strict<type_mn>&&) = delete;

   template<typename Type>
   const Type& read_trivial(const bool unaligned = false)
   {
      static_assert(std::is_trivially_copyable_v<Type>,
                    "Type must be trivially copyable.");
      static_assert(!std::is_reference_v<Type>, "Type can not be a reference.");
      static_assert(!std::is_pointer_v<Type>, "Type can not be a pointer.");

      const auto cur_pos = _head;
      _head += sizeof(Type);

      check_head();

      if (!unaligned) align_head();

      return view_type_as<Type>(_data[cur_pos]);
   }

   template<typename Type>
   const Type& read_trivial_unaligned()
   {
      return read_trivial<Type>(true);
   }

   template<typename Type>
   auto read_array(typename const gsl::span<const Type>::index_type size,
                   const bool unaligned = false) -> gsl::span<const Type>
   {
      static_assert(std::is_trivially_copyable_v<Type>,
                    "Type must be trivially copyable.");
      static_assert(!std::is_reference_v<Type>, "Type can not be a reference.");
      static_assert(!std::is_pointer_v<Type>, "Type can not be a pointer.");

      const auto cur_pos = _head;
      _head += sizeof(Type) * size;

      check_head();

      if (!unaligned) align_head();

      return {&view_type_as<Type>(_data[cur_pos]), size};
   }

   template<typename Type>
   auto read_array_unaligned(typename const gsl::span<const Type>::index_type size)
      -> gsl::span<const Type>
   {
      return read_array<Type>(size, true);
   }

   template<typename Char_type = char>
   auto read_string(const bool unaligned = false) -> std::basic_string_view<Char_type>
   {
      const Char_type* const string = reinterpret_cast<const Char_type*>(_data + _head);
      const auto string_length = cstring_length(string, _size - _head);

      _head += (string_length + 1) * sizeof(Char_type);

      check_head();

      if (!unaligned) align_head();

      return {string, string_length};
   }

   template<typename Char_type = char>
   auto read_string_unaligned() -> std::basic_string_view<Char_type>
   {
      return read_string<Char_type>(true);
   }

   Ucfb_reader read_child(const bool unaligned = false);

   Ucfb_reader read_child_unaligned()
   {
      return read_child(true);
   }

   template<Magic_number type_mn>
   auto read_child_strict(const bool unaligned = false) -> Ucfb_reader_strict<type_mn>
   {
      return {read_child_strict_impl(type_mn, unaligned),
              Ucfb_reader_strict<type_mn>::Unchecked_tag{}};
   }

   template<Magic_number type_mn>
   auto read_child_strict_unaligned() -> Ucfb_reader_strict<type_mn>
   {
      return read_child_strict<type_mn>(true);
   }

   void consume(const std::size_t amount, const bool unaligned = false);

   void consume_unaligned(const std::size_t amount)
   {
      consume(amount, true);
   }

   explicit operator bool() const noexcept;

   void reset_head() noexcept;

   Magic_number magic_number() const noexcept;

   std::size_t size() const noexcept;

   // Temporary function for use as a compatibility shim. This *will* be removed
   // as soon as all code has been migrated away from using raw structs.
   template<typename Type>
   [[deprecated("Replace with ucfb_reader abstraction.")]] const Type& view_as_chunk()
      const noexcept
   {
      return view_type_as<Type>(_data[-8]);
   }

private:
   // Special constructor for use by read_child, performs no error checking.
   Ucfb_reader(const Magic_number mn, const std::uint32_t size, const Byte* const data);

   Ucfb_reader read_child_strict_impl(const Magic_number child_mn, const bool unaligned);

   void check_head();

   void align_head() noexcept
   {
      const auto remainder = _head % 4;

      if (remainder != 0) _head += (4 - remainder);
   }

   const Magic_number _mn;
   const std::size_t _size;
   const Byte* const _data;

   std::size_t _head = 0;
};

template<Magic_number type_mn>
class Ucfb_reader_strict : public Ucfb_reader {
public:
   Ucfb_reader_strict() = delete;

   explicit Ucfb_reader_strict(Ucfb_reader ucfb_reader) noexcept
      : Ucfb_reader{ucfb_reader}
   {
      Expects(type_mn == ucfb_reader.magic_number());
   }

private:
   friend class Ucfb_reader;

   struct Unchecked_tag {
   };

   Ucfb_reader_strict(Ucfb_reader ucfb_reader, Unchecked_tag) : Ucfb_reader{ucfb_reader}
   {
   }
};