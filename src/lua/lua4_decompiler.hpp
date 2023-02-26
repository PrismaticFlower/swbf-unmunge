#pragma once

#include "lua/lua4.hpp"
#include "ucfb_reader.hpp"

#include <string_view>
#include <variant>

using Lua4String = std::basic_string_view<char>;

struct Lua4_local {
   Lua4Int line_defined;
   Lua4Int line_destroyed;
   Lua4String name;
};

//! Similar to the Lua Protocol (Proto* )
struct Lua4_chunk {
   Lua4Int line;
   Lua4Int parameters;
   bool is_variadic;
   Lua4Int max_stack_size;

   std::vector<Lua4_local> locals;
   std::vector<Lua4Int> lines;
   std::vector<Lua4Int> instructions;
   std::vector<Lua4String> constants;
   std::vector<Lua4Number> numbers;
   std::vector<Lua4_chunk> functions;

   Lua4String name;
};

//! Recreates the Lua AST and stores the script content
struct Lua4_state {
   unsigned indent = 0;
   std::stringstream buffer;
   std::vector<std::string> stack;
   bool closure = false;
};

auto handle_lua4_function(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c)
   -> Lua4_chunk;
void handle_lua4_locals(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c);
void handle_lua4_lines(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c);
void handle_lua4_constants(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c);
void handle_lua4_code(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c);

void process_code(const Lua4_chunk& chunk, Lua4_state& state);

auto handle_lua4_string(Ucfb_reader& script, const Lua4_header& header)
   -> Lua4String;

void create_function(Lua4_state& state);
