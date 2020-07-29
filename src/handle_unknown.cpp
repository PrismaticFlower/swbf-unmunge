
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

#include <atomic>
#include <optional>
#include <string>

namespace {

using namespace std::literals;

std::string get_unique_chunk_name() noexcept
{
   static std::atomic_int64_t chunk_count{0};

   std::string result{"chunk_"s};

   result += std::to_string(chunk_count.fetch_add(1));

   return result;
}
}

void handle_unknown(Ucfb_reader chunk, File_saver& file_saver,
                    std::optional<std::string_view> file_name,
                    std::optional<std::string_view> file_extension)
{
   std::string file;
   file.reserve(chunk.size() + 16);

   file += "ucfb"sv;
   file += view_object_as_string(static_cast<std::uint32_t>(chunk.size() + 8));
   file += view_object_as_string(chunk.magic_number());
   file += view_object_as_string(static_cast<std::uint32_t>(chunk.size()));
   file += view_object_span_as_string(chunk.read_bytes(chunk.size()));

   file_saver.save_file(file, "munged", file_name ? *file_name : get_unique_chunk_name(),
                        file_extension ? *file_extension : ".munged"sv);
}
