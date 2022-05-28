#include "chunk_handlers.hpp"
#include "lua/lua4_decompiler.hpp"
#include "synced_cout.hpp"

#include <string_view>

using namespace std::literals;

void handle_lua4_script(Ucfb_reader& script, File_saver& file_saver, Lua4_chunk& chunk)
{
   const auto lua_header = script.read_trivial_unaligned<Lua4_header>();

   // Check if the test number is correct
   const auto test_number_buffer =
      script.read_bytes_unaligned(lua_header.size_number_bytes);
   const auto test_number = *reinterpret_cast<const float*>(test_number_buffer.data());

   if (fabs(test_number - LUA4_TEST_NUMBER) > std::numeric_limits<float>::epsilon()) {
      throw std::runtime_error{"Test Number is not valid."};
   }

   handle_lua4_function(script, lua_header, chunk);
}

void handle_lua_script(Ucfb_reader& script, File_saver& file_saver)
{
   const auto lua_version = script.read_trivial_unaligned<char>();

   if (lua_version == 0x40) {
      Lua4_chunk chunk;
      handle_lua4_script(script, file_saver, chunk);

      // We have the lua binary stored in the Lua4_chunk structure. We can now process the
      // instructions and try to reassemble the code in lua syntax.
      Lua4_state state;
      process_code(chunk, state);

      synced_cout::print(state.buffer.str() + "\n\n");
   }
}

void handle_script_body(Ucfb_reader body, File_saver& file_saver)
{
   const auto script_header = body.read_trivial_unaligned<Magic_number>();

   if (script_header == "\x1BLua"_mn) {
      handle_lua_script(body, file_saver);
   }
}

void handle_script(Ucfb_reader script, File_saver& file_saver)
{
   const auto name = script.read_child_strict<"NAME"_mn>().read_string();

   while (script) {
      const auto child = script.read_child();
      if (child.magic_number() == "INFO"_mn) {
         // pass
      }
      else if (child.magic_number() == "BODY"_mn) {
         handle_script_body(child, file_saver);
      }
   }

   script.reset_head();

   handle_unknown(script, file_saver, name, ".script"sv);
}