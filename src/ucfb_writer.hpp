#pragma once

#include "magic_number.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gsl/gsl>

template<typename Last_act, typename Writer_type>
class Ucfb_writer_child : public Writer_type {
   static_assert(std::is_invocable_v<Last_act, decltype(Writer_type::_size)>);

public:
   Ucfb_writer_child(Last_act last_act, std::ostream& output_stream,
                     const Magic_number mn)
      : Writer_type{output_stream, mn}, _last_act{std::move(last_act)}
   {
   }

   Ucfb_writer_child(const Ucfb_writer_child&) = delete;
   Ucfb_writer_child& operator=(const Ucfb_writer_child&) = delete;

   Ucfb_writer_child(Ucfb_writer_child&&) = delete;
   Ucfb_writer_child& operator=(Ucfb_writer_child&&) = delete;

   ~Ucfb_writer_child()
   {
      _last_act(Writer_type::_size);
   }

private:
   Last_act _last_act;
};

class Ucfb_writer {
public:
   enum class Alignment : bool { aligned, unaligned };

   Ucfb_writer(std::ostream& output_stream, const Magic_number root_mn = "ucfb"_mn)
      : _out{output_stream}
   {
      Expects(output_stream.good());

      _out.write(reinterpret_cast<const char*>(&root_mn), sizeof(Magic_number));

      _size_pos = _out.tellp();
      _out.write(reinterpret_cast<const char*>(&size_place_hold),
                 sizeof(size_place_hold));
   }

   ~Ucfb_writer()
   {
      const auto cur_pos = _out.tellp();

      const auto chunk_size = static_cast<std::int32_t>(_size);

      _out.seekp(_size_pos);
      _out.write(reinterpret_cast<const char*>(&chunk_size), sizeof(chunk_size));
      _out.seekp(cur_pos);
   }

   Ucfb_writer() = delete;

   Ucfb_writer(const Ucfb_writer&) = delete;
   Ucfb_writer& operator=(const Ucfb_writer&) = delete;

   Ucfb_writer(Ucfb_writer&&) = delete;
   Ucfb_writer& operator=(Ucfb_writer&&) = delete;

   auto emplace_child(const Magic_number mn)
   {
      const auto last_act = [this](auto child_size) { increase_size(child_size); };

      align_file();

      increase_size(8);

      return Ucfb_writer_child<decltype(last_act), Ucfb_writer>{last_act, _out, mn};
   }

   template<typename Type>
   void write(const Type& value, Alignment alignment = Alignment::aligned)
   {
      static_assert(std::is_trivially_copyable_v<Type>);

      _out.write(reinterpret_cast<const char*>(&value), sizeof(Type));

      increase_size(sizeof(Type));

      if (alignment == Alignment::aligned) align_file();
   }

   template<typename Type>
   void write(const gsl::span<Type> span, Alignment alignment = Alignment::aligned)
   {
      static_assert(std::is_trivially_copyable_v<Type>);

      _out.write(reinterpret_cast<const char*>(span.data()), span.size_bytes());
      increase_size(span.size_bytes());

      if (alignment == Alignment::aligned) align_file();
   }

   void write(const std::string_view string, Alignment alignment = Alignment::aligned)
   {
      _out.write(string.data(), string.size());
      _out.put('\0');

      increase_size(string.length() + 1ll);

      if (alignment == Alignment::aligned) align_file();
   }


   /*
   Workaround while I decrypt error messages and learn templates
   
   void write(const std::string_view string, Alignment alignment = Alignment::aligned)
   {
      _out.write(string.data(), string.size());
      _out.put('\0');

      increase_size(string.length() + 1ll);

      if (alignment == Alignment::aligned) align_file();
   }
   */

   void write(const std::string& string, Alignment alignment = Alignment::aligned)
   {
      write(std::string_view{string}, alignment);
   }

   template<typename... Args>
   auto write(Args&&... args)
      -> std::enable_if_t<!std::disjunction_v<std::is_same<Alignment, Args>...>>
   {
      (this->write(args, Alignment::aligned), ...);
   }

   template<typename... Args>
   auto write_unaligned(Args&&... args)
      -> std::enable_if_t<!std::disjunction_v<std::is_same<Alignment, Args>...>>
   {
      (this->write(args, Alignment::unaligned), ...);
   }

   auto pad(const std::uint32_t amount, Alignment alignment = Alignment::aligned)
   {
      for (auto i = 0u; i < amount; ++i) _out.put('\0');

      increase_size(amount);

      if (alignment == Alignment::aligned) align_file();
   }

   auto pad_unaligned(const std::uint32_t amount)
   {
      return pad(amount, Alignment::unaligned);
   }

   auto absolute_size() const noexcept -> std::uint32_t
   {
      return gsl::narrow_cast<std::uint32_t>(_out.tellp());
   }

private:
   template<typename Last_act, typename Writer_type>
   friend class Ucfb_writer_child;

   constexpr static std::uint32_t size_place_hold = 0;

   void align_file()
   {
      const auto remainder = _size % 4;

      if (remainder != 0) {
         _out.write("\0\0\0\0", (4 - remainder));

         increase_size(4 - remainder);
      }
   }

   void increase_size(const std::int64_t len)
   {
      _size += len;

      if (_size > std::numeric_limits<std::int32_t>::max()) {
         throw std::runtime_error{"ucfb file too large!"};
      }

      Ensures(_size <= std::numeric_limits<std::int32_t>::max());
   }

   std::ostream& _out;
   std::streampos _size_pos;
   std::int64_t _size{};
};
