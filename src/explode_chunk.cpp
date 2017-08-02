
#include "explode_chunk.hpp"
#include "type_pun.hpp"

#include "tbb/parallel_for.h"

#include <vector>

namespace {

inline bool is_usable_chunk_name(const Magic_number magic_number) noexcept
{
   constexpr auto safe_chars =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"_sv;

   const auto string = view_pod_as_string(magic_number);

   for (const auto& c : string) {
      if (safe_chars.find(c) == safe_chars.npos) return false;
   }

   return true;
}

inline bool is_possible_parent(Ucfb_reader chunk)
{
   if (!is_usable_chunk_name(chunk.magic_number()) && chunk.size() == 0) return false;

   return true;
}

inline bool is_possible_child(Ucfb_reader chunk)
{
   if (!is_usable_chunk_name(chunk.magic_number()) && chunk.size() <= 3) return false;

   return true;
}

inline std::string get_chunk_name(const Ucfb_reader& chunk, const std::size_t index)
{
   std::string name;
   name += std::to_string(index);

   name += '_';

   if (is_usable_chunk_name(chunk.magic_number())) {
      name += view_pod_as_string(chunk.magic_number());
   }
   else {
      name += serialize_magic_number(chunk.magic_number());
   }

   name += '_';

   name += std::to_string(chunk.size());

   return name;
}

void write_child_chunks(const std::vector<Ucfb_reader>& children, File_saver& file_saver)
{
   tbb::parallel_for(std::size_t{0u}, children.size(),
                     [&](auto i) { explode_chunk(children[i], file_saver, i); });
}

void write_data_chunk(Ucfb_reader chunk, File_saver& file_saver, const std::size_t index)
{
   const auto name = get_chunk_name(chunk, index);

   const auto data = chunk.read_array_unaligned<char>(chunk.size());

   std::string buffer;
   buffer.reserve(data.size() + 8);
   buffer += view_pod_as_string(chunk.magic_number());
   buffer += view_pod_as_string(static_cast<std::uint32_t>(chunk.size()));
   buffer += view_pod_span_as_string(data);

   file_saver.save_file(buffer, "", name, ".chunk");
}
}

void explode_chunk(Ucfb_reader chunk, File_saver& file_saver, const std::size_t index)
{
   if (!is_possible_parent(chunk)) return write_data_chunk(chunk, file_saver, index);

   std::vector<Ucfb_reader> children;
   children.reserve(32);

   while (chunk) {
      const auto child = chunk.read_child(std::nothrow);

      if (!child || !is_possible_child(*child)) {
         chunk.reset_head();

         return write_data_chunk(chunk, file_saver, index);
      }

      children.emplace_back(*child);
   }

   const auto name = get_chunk_name(chunk, index);

   auto nested_saver = file_saver.create_nested(name);

   write_child_chunks(children, nested_saver);
}