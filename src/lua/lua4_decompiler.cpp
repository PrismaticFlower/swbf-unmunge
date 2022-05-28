#include "lua4_decompiler.hpp"
#include "synced_cout.hpp"
#include "util_dump.hpp"
#include <bitset>

//! \brief  Reads a Lua4 function from byte code.
//!
//!         Every Lua script has a main function that consists of multiple sub-functions.
//!         Within every functions there exist the following basic building blocks:
//!         - Locals:
//!             - Number of locals
//!             - List of locals
//!                 - Name of the local
//!                 - Source line where it is defined
//!                 - Source line where it is destroyed
//!         - Lines: (debug information)
//!             - Number of Lines
//!             - List of line info
//!                 - Mapping of instructions to source lines (for debugging)
//!         - Constants:
//!             - Number of constants
//!             - List of constants
//!                 - Name of the constant
//!                 - Number of numbers used by the constant
//!                 - List of numbers
//!                     - The number
//!                 - Number of functions in this constant
//!                     - Definition of the functions within this constant (recursive)
//!         - Code:
//!             - Number of instructions
//!             - List of instructions
//!                 - The instruction (fixed-size int)
//!
auto handle_lua4_function(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c)
   -> Lua4_chunk
{
   // Read length of function name
   script.read_bytes_unaligned(header.size_size_t_bytes);
   const auto name = script.read_string_unaligned();

   c.line = script.read_trivial_unaligned<Lua4Int>();
   c.parameters = script.read_trivial_unaligned<Lua4Int>();
   c.is_variadic = script.read_trivial_unaligned<std::uint8_t>();
   c.max_stack_size = script.read_trivial_unaligned<Lua4Int>();

   handle_lua4_locals(script, header, c);

   handle_lua4_lines(script, header, c);

   handle_lua4_constants(script, header, c);

   handle_lua4_code(script, header, c);

   return c;
}

void handle_lua4_locals(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c)
{
   const auto variables = script.read_trivial_unaligned<Lua4Int>();
   c.locals.reserve(variables);

   for (unsigned i = 0; i < variables; ++i) {
      c.locals.push_back({script.read_trivial_unaligned<Lua4Int>(),
                          script.read_trivial_unaligned<Lua4Int>(),
                          handle_lua4_string(script, header)});
   }
}

void handle_lua4_code(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c)
{
   const auto num_instructions = script.read_trivial_unaligned<Lua4Int>();
   const auto instructions = script.read_array_unaligned<Lua4Int>(num_instructions);
   c.instructions.reserve(num_instructions);
   c.instructions.insert(c.instructions.end(), instructions.begin(), instructions.end());
}

void handle_lua4_lines(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c)
{
   const auto num_lines = script.read_trivial_unaligned<Lua4Int>();
   const auto lines = script.read_array_unaligned<Lua4Int>(num_lines);
   c.lines.reserve(num_lines);
   c.lines.insert(c.lines.end(), lines.begin(), lines.end());
}

void handle_lua4_constants(Ucfb_reader& script, const Lua4_header& header, Lua4_chunk& c)
{
   const auto variables = script.read_trivial_unaligned<Lua4Int>();
   c.constants.reserve(variables);

   for (unsigned i = 0; i < variables; ++i) {
      c.constants.emplace_back(handle_lua4_string(script, header));
   }

   const auto num_numbers = script.read_trivial_unaligned<Lua4Int>();
   const auto numbers = script.read_array_unaligned<Lua4Number>(num_numbers);
   c.numbers.reserve(num_numbers);
   c.numbers.insert(c.numbers.end(), numbers.begin(), numbers.end());

   const auto functions = script.read_trivial_unaligned<Lua4Int>();
   for (unsigned i = 0; i < functions; ++i) {
      Lua4_chunk child;
      c.functions.emplace_back(handle_lua4_function(script, header, child));
   }
}

auto handle_lua4_string(Ucfb_reader& script, const Lua4_header& header)
   -> std::basic_string_view<char>
{
   const auto len = script.read_trivial_unaligned<Lua4Int>();
   if (len)
      return script.read_string_unaligned();
   else
      return "";
}

void process_code(const Lua4_chunk& chunk, Lua4_state& state)
{
   int i = 0;
   for(const auto& number: chunk.numbers)
   {
      synced_cout::print(i, ":", number, "\n");
      i++;
   }
   i = 0;
   for(const auto& g: chunk.constants)
   {
      synced_cout::print(i, ":", g, "\n");
      i++;
   }

   state.buffer << std::string(state.indent, ' ') << "function " << chunk.name << "()\n";

   for(const auto& child : chunk.functions) {
      //Lua4_state child_state;
      state.indent += 2;
      process_code(child, state);
      state.indent -= 2;
   }
   
   for (const auto& instruction : chunk.instructions) {
      const auto op = get_OP(instruction);
      const auto U = get_U(instruction);
      const auto S = get_S(instruction);
      const auto A = get_A(instruction);
      const auto B = get_B(instruction);

      std::bitset<32> b(instruction);
      synced_cout::print(b, " ");

      switch (op) {
      case Lua4_OP_code::OP_END: {
         synced_cout::print("OP_END ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n"); 
         state.buffer << std::string(state.indent, ' ') << "end\n";
         break;
      }
      case Lua4_OP_code::OP_RETURN: {
         synced_cout::print("OP_RETURN ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n"); 
         state.buffer << std::string(state.indent, ' ') << "return " << U << "\n";
         break;
      }
      case Lua4_OP_code::OP_CALL: {
         synced_cout::print("OP_CALL ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         
         state.buffer << std::string(state.indent, ' ') << "()\n";
         break;
      }
      case Lua4_OP_code::OP_TAILCALL: {
         synced_cout::print("OP_TAILCALL ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHNIL: {
         synced_cout::print("OP_PUSHNIL ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << "nil,";
         break;
      }
      case Lua4_OP_code::OP_POP: {
         synced_cout::print("OP_POP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHINT: {
         synced_cout::print("OP_PUSHINT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << B << ",";
         break;
      }
      case Lua4_OP_code::OP_PUSHSTRING: {
         synced_cout::print("OP_PUSHSTRING ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << "\"" << chunk.constants[U] << "\",";
         break;
      }
      case Lua4_OP_code::OP_PUSHNUM: {
         synced_cout::print("OP_PUSHNUM ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << chunk.numbers[U] << ",";
         break;
      }
      case Lua4_OP_code::OP_PUSHNEGNUM: {
         synced_cout::print("OP_PUSHNEGNUM ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << -chunk.numbers[U] << ",";
         break;
      }
      case Lua4_OP_code::OP_PUSHUPVALUE: {
         synced_cout::print("OP_PUSHUPVALUE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_GETLOCAL: {
         synced_cout::print("OP_GETLOCAL ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << "LOCAL" << ",";
         //state.buffer << std::string(state.indent, ' ') << state.locals[U] << ",";
         break;
      }
      case Lua4_OP_code::OP_GETGLOBAL: {
         synced_cout::print("OP_GETGLOBAL ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << chunk.constants[U] <<",";
         break;
      }
      case Lua4_OP_code::OP_GETTABLE: {
         synced_cout::print("OP_GETTABLE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_GETDOTTED: {
         synced_cout::print("OP_GETDOTTED ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_GETINDEXED: {
         synced_cout::print("OP_GETINDEXED ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHSELF: {
         synced_cout::print("OP_PUSHSELF ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_CREATETABLE: {
         synced_cout::print("OP_CREATETABLE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_SETLOCAL: {
         synced_cout::print("OP_SETLOCAL ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         //state.locals.emplace_back(chunk.constants[U]);
         state.buffer << std::string(state.indent, ' ') << chunk.constants[U] << "= \n";
         break;
      }
      case Lua4_OP_code::OP_SETGLOBAL: {
         synced_cout::print("OP_SETGLOBAL ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         state.buffer << std::string(state.indent, ' ') << chunk.constants[U] << "= \n";
         break;
      }
      case Lua4_OP_code::OP_SETTABLE: {
         synced_cout::print("OP_SETTABLE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_SETLIST: {
         synced_cout::print("OP_SETLIST ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_SETMAP: {
         synced_cout::print("OP_SETMAP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_ADD: {
         synced_cout::print("OP_ADD ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_ADDI: {
         synced_cout::print("OP_ADDI ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_SUB: {
         synced_cout::print("OP_SUB ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_MULT: {
         synced_cout::print("OP_MULT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_DIV: {
         synced_cout::print("OP_DIV ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_POW: {
         synced_cout::print("OP_POW ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_CONCAT: {
         synced_cout::print("OP_CONCAT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_MINUS: {
         synced_cout::print("OP_MINUS ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_NOT: {
         synced_cout::print("OP_NOT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPNE: {
         synced_cout::print("OP_JMPNE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPEQ: {
         synced_cout::print("OP_JMPEQ ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPLT: {
         synced_cout::print("OP_JMPLT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPLE: {
         synced_cout::print("OP_JMPLE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPGT: {
         synced_cout::print("OP_JMPGT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPGE: {
         synced_cout::print("OP_JMPGE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPT: {
         synced_cout::print("OP_JMPT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPF: {
         synced_cout::print("OP_JMPF ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPONT: {
         synced_cout::print("OP_JMPONT ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPONF: {
         synced_cout::print("OP_JMPONF ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMP: {
         synced_cout::print("OP_JMP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHNILJMP: {
         synced_cout::print("OP_PUSHNILJMP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_FORPREP: {
         synced_cout::print("OP_FORPREP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_FORLOOP: {
         synced_cout::print("OP_FORLOOP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_LFORPREP: {
         synced_cout::print("OP_LFORPREP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");

         break;
      }
      case Lua4_OP_code::OP_LFORLOOP: {
         synced_cout::print("OP_LFORLOOP ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         break;
      }
      case Lua4_OP_code::OP_CLOSURE: {
         synced_cout::print("OP_CLOSURE ", (unsigned)U, " ", (int)S, " ", (unsigned)A, " ", (unsigned)B, "\n");
         //state.buffer << std::string(state.indent, ' ') << "()\n";
         break;
      }

      default:
         throw std::runtime_error{"Unrecognized OP code: " + std::to_string(op)};
      }
   }

   // synced_cout::print(buf);
}
