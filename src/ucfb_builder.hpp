#pragma once

#include "magic_number.hpp"
#include "type_pun.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

class Ucfb_builder {
public:
   using iterator = std::vector<Ucfb_builder>::iterator;
   using const_iterator = std::vector<Ucfb_builder>::const_iterator;

   explicit Ucfb_builder(Magic_number magic_number);

   Ucfb_builder(const std::filesystem::path& file_path, Magic_number magic_number);

   Magic_number get_magic_number() const noexcept;

   void add_child(const Ucfb_builder& child);

   void add_child(Ucfb_builder&& child);

   Ucfb_builder& emplace_child(Magic_number magic_number);

   iterator begin() noexcept
   {
      return _children.begin();
   }

   iterator end() noexcept
   {
      return _children.end();
   }

   const_iterator begin() const noexcept
   {
      return _children.begin();
   }

   const_iterator end() const noexcept
   {
      return _children.end();
   }

   const_iterator cbegin() const noexcept
   {
      return _children.cbegin();
   }

   const_iterator cend() const noexcept
   {
      return _children.cend();
   }

   void write(std::string_view str, bool null_terminate = true, bool aligned = true);

   template<typename Type, typename = std::enable_if_t<std::is_trivially_copyable_v<Type>>>
   void write(const Type& pod)
   {
      _contents += view_object_as_string(pod);
   }

   template<typename... Pod_types>
   void write_multiple(Pod_types&&... pods)
   {
      const bool unused[] = {false, (write(std::forward<Pod_types>(pods)), false)...};
   }

   void pad_till_aligned();

   std::string create_buffer() const;

private:
   std::size_t calc_size() const noexcept;

   Magic_number _magic_number;
   std::string _contents;
   std::vector<Ucfb_builder> _children;
};
