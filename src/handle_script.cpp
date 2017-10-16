
#include "chunk_handlers.hpp"
#include "file_saver.hpp"
#include "string_helpers.hpp"

namespace {
void handle_script_keep_munged(Ucfb_reader script, File_saver& file_saver)
{
   const auto name = script.read_child_strict<"NAME"_mn>().read_string();

   script.reset_head();

   handle_unknown(script, file_saver, name, ".script"_sv);
}
}

void handle_script(Ucfb_reader script, File_saver& file_saver, bool keep_munged)
{
   if (keep_munged) handle_script_keep_munged(script, file_saver);

   // TODO: Decompilation goes here...
}
