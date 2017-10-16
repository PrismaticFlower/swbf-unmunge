
#include "chunk_handlers.hpp"
#include "file_saver.hpp"
#include "string_helpers.hpp"

void handle_script(Ucfb_reader script, File_saver& file_saver)
{
   const auto name = script.read_child_strict<"NAME"_mn>().read_string();

   script.reset_head();

   handle_unknown(script, file_saver, name, ".script"_sv);
}

void handle_shader(Ucfb_reader shader, File_saver& file_saver)
{
   const auto rtyp = shader.read_child_strict<"RTYP"_mn>().read_string();

   shader.reset_head();

   handle_unknown(shader, file_saver, rtyp, ".shader"_sv);
}

void handle_font(Ucfb_reader font, File_saver& file_saver)
{
   const auto name = font.read_child_strict<"NAME"_mn>().read_string();

   font.reset_head();

   handle_unknown(font, file_saver, name, ".font"_sv);
}

void handle_binary(Ucfb_reader binary, File_saver& file_saver, std::string_view extension)
{
   const auto name = binary.read_child_strict<"NAME"_mn>().read_string();

   binary.reset_head();

   handle_unknown(binary, file_saver, name, extension);
}

void handle_zaabin(Ucfb_reader zaabin, File_saver& file_saver)
{
   const auto name = zaabin.read_child_strict<"NAME"_mn>().read_string();

   zaabin.reset_head();

   handle_unknown(zaabin, file_saver, name, ".zaabin"_sv);

   file_saver.save_file("ucft\n{\n}"_sv, "munged"_sv, name, ".anims");
}
