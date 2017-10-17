
#include "chunk_handlers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"

namespace {

const auto luabin_magic_number = "\x1BLua"_mn;

struct Luabin_header {
   Magic_number magic_number;
   std::uint8_t version;
   bool little_endian;

   std::uint8_t int_size;
   std::uint8_t sizet_size;
   std::uint8_t instruction_size;

   std::uint8_t op_field_bits;
   std::uint8_t a_field_bits;
   std::uint8_t b_field_bits;
   std::uint8_t c_field_bits;

   std::uint8_t number_size;

   // Variable size test number goes here.
};

void handle_script_keep_munged(Ucfb_reader script, File_saver& file_saver)
{
   const auto name = script.read_child_strict<"NAME"_mn>().read_string();

   script.reset_head();

   handle_unknown(script, file_saver, name, ".script"_sv);
}

bool header_compatible(const Luabin_header header) noexcept
{
   if (header.magic_number != luabin_magic_number) return false;
   if (header.version != 0x50ui8) return false;
   if (!header.little_endian) return false;

   if (header.int_size != 4) return false;
   if (header.sizet_size != 4) return false;
   if (header.instruction_size != 4) return false;

   if (header.op_field_bits != 6) return false;
   if (header.a_field_bits != 8) return false;
   if (header.b_field_bits != 9) return false;
   if (header.c_field_bits != 9) return false;

   if (header.number_size != 4) return false;

   return true;
}

Luabin_header read_luabin_header(Ucfb_reader& body)
{
   Luabin_header header;

   header.magic_number = body.read_trivial_unaligned<Magic_number>();
   header.version = body.read_trivial_unaligned<std::uint8_t>();
   static_assert(sizeof(bool) == sizeof(std::uint8_t));
   header.little_endian = body.read_trivial_unaligned<bool>();

   header.int_size = body.read_trivial_unaligned<std::uint8_t>();
   header.sizet_size = body.read_trivial_unaligned<std::uint8_t>();
   header.instruction_size = body.read_trivial_unaligned<std::uint8_t>();

   header.op_field_bits = body.read_trivial_unaligned<std::uint8_t>();
   header.a_field_bits = body.read_trivial_unaligned<std::uint8_t>();
   header.b_field_bits = body.read_trivial_unaligned<std::uint8_t>();
   header.c_field_bits = body.read_trivial_unaligned<std::uint8_t>();
   header.number_size = body.read_trivial_unaligned<std::uint8_t>();

   return header;
}
}

void handle_script(Ucfb_reader script, File_saver& file_saver, bool keep_munged)
{
   if (keep_munged) return handle_script_keep_munged(script, file_saver);

   const auto name = script.read_child_strict<"NAME"_mn>().read_string();
   script.read_child_strict<"INFO"_mn>();
   auto body = script.read_child_strict<"BODY"_mn>();

   const auto header = read_luabin_header(body);

   if (!header_compatible(header)) {
      script.reset_head();
      return handle_script_keep_munged(script, file_saver);
   }

   std::string buffer;

   buffer += "-- script name: "_sv;
   buffer += name;
   buffer += "\n\n "_sv;

   // TODO: Decompilation goes here...

   file_saver.save_file(buffer, "scripts"_sv, name, ".lua"_sv);
}
