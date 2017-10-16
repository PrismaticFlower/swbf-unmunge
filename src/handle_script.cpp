
#include "chunk_handlers.hpp"
#include "file_saver.hpp"
#include "string_helpers.hpp"

void handle_script(Ucfb_reader script, File_saver& file_saver)
{
   const auto name = script.read_child_strict<"NAME"_mn>().read_string();

   script.reset_head();

   handle_unknown(script, file_saver, name, ".script"_sv);
}
