#pragma once

#include "byte.hpp"
#include "chunk_headers.hpp"
#include "magic_number.hpp"
#include "type_pun.hpp"

#include <cstddef>
#include <string>
#include <type_traits>

class Ucfb_reader {
public:
   Ucfb_reader() = delete;
   Ucfb_reader(const Byte* bytes, const std::uint32_t size);

   template<typename Type>
   const Type& read_trivial(bool unaligned = false)
   {
      static_assert(std::is_trivially_copyable_v<Type>,
                    "Type must be trivially copyable.");
      static_assert(!std::is_reference_v<Type>, "Type can not be a reference.");
      static_assert(!std::is_pointer_v<Type>, "Type can not be a pointer.");

      const auto cur_pos = _head;
      _head += sizeof(Type);

      if (_head > _size) {
         throw std::runtime_error{"Attempt to read past end of chunk."};
      }

      if (!unaligned) align_head();

      return view_type_as<Type>(_data[cur_pos]);
   }

   template<typename Type>
   const Type& read_trivial_unaligned()
   {
      return read_trivial<Type>(true);
   }

   Ucfb_reader read_child(bool unaligned = false);

   Ucfb_reader read_child_unaligned()
   {
      return read_child(true);
   }

   explicit operator bool() const noexcept;

   void reset_head() noexcept;

   Magic_number magic_number() const noexcept;

   // Temporary function for use as a compatibility shim. This *will* be removed
   // as soon as all code has been migrated away from using raw structs.
   [[deprecated("Replace with ucfb_reader abstraction.")]] const chunks::Unknown&
   view_as_chunk() const noexcept;

private:
   // Special constructor for use by read_child, performs no error checking.
   Ucfb_reader(const Magic_number mn, const std::uint32_t size, const Byte* const data);

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
