#include "lua4_decompiler.hpp"
#include "synced_cout.hpp"
#include <bitset>

const bool debug = true;
template<typename... Args>
void debug_print(Args... args)
{
   if (debug) synced_cout::print(args...);
}

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
   for (const auto& child : chunk.functions) {
      // Lua4_state child_state;
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
      const auto F = (float)(instruction);

      switch (op) {
      case Lua4_OP_code::OP_END: {
         debug_print("OP_END ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.buffer << std::string(state.indent, ' ') << "end\n";
         break;
      }
      case Lua4_OP_code::OP_RETURN: {
         debug_print("OP_RETURN ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.buffer << std::string(state.indent, ' ') << "return " << U << "\n";
         break;
      }
      case Lua4_OP_code::OP_CALL: {
         debug_print("OP_CALL ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         create_function(state);
         break;
      }
      case Lua4_OP_code::OP_TAILCALL: {
         debug_print("OP_TAILCALL ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHNIL: {
         debug_print("OP_PUSHNIL ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.stack.emplace_back("nil");
         break;
      }
      case Lua4_OP_code::OP_POP: {
         debug_print("OP_POP ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHINT: {
         debug_print("OP_PUSHINT ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.stack.emplace_back(std::to_string(B));
         break;
      }
      case Lua4_OP_code::OP_PUSHSTRING: {
         debug_print("OP_PUSHSTRING ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.stack.emplace_back(
            std::string("\"").append(chunk.constants[U]).append("\""));
         break;
      }
      case Lua4_OP_code::OP_PUSHNUM: {
         debug_print("OP_PUSHNUM ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.stack.emplace_back(std::to_string(chunk.numbers[U]));
         break;
      }
      case Lua4_OP_code::OP_PUSHNEGNUM: {
         debug_print("OP_PUSHNEGNUM ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.stack.emplace_back(std::to_string(-chunk.numbers[U]));
         break;
      }
      case Lua4_OP_code::OP_PUSHUPVALUE: {
         debug_print("OP_PUSHUPVALUE ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_GETLOCAL: {
         debug_print("OP_GETLOCAL ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.stack.emplace_back(std::to_string(U));
         break;
      }
      case Lua4_OP_code::OP_GETGLOBAL: {
         debug_print("OP_GETGLOBAL ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.stack.emplace_back(chunk.constants[U]);
         break;
      }
      case Lua4_OP_code::OP_GETTABLE: {
         debug_print("OP_GETTABLE ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_GETDOTTED: {
         debug_print("OP_GETDOTTED ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_GETINDEXED: {
         debug_print("OP_GETINDEXED ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHSELF: {
         debug_print("OP_PUSHSELF ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_CREATETABLE: {
         debug_print("OP_CREATETABLE ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_SETLOCAL: {
         debug_print("OP_SETLOCAL ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         break;
      }
      case Lua4_OP_code::OP_SETGLOBAL: {
         debug_print("OP_SETGLOBAL ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         state.buffer << std::string(state.indent, ' ') << chunk.constants[U] << "= \n";
         break;
      }
      case Lua4_OP_code::OP_SETTABLE: {
         debug_print("OP_SETTABLE ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_SETLIST: {
         debug_print("OP_SETLIST ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_SETMAP: {
         debug_print("OP_SETMAP ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_ADD: {
         debug_print("OP_ADD ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_ADDI: {
         debug_print("OP_ADDI ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_SUB: {
         debug_print("OP_SUB ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_MULT: {
         debug_print("OP_MULT ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_DIV: {
         debug_print("OP_DIV ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_POW: {
         debug_print("OP_POW ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_CONCAT: {
         debug_print("OP_CONCAT ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_MINUS: {
         debug_print("OP_MINUS ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_NOT: {
         debug_print("OP_NOT ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPNE: {
         debug_print("OP_JMPNE ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPEQ: {
         debug_print("OP_JMPEQ ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPLT: {
         debug_print("OP_JMPLT ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPLE: {
         debug_print("OP_JMPLE ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPGT: {
         debug_print("OP_JMPGT ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPGE: {
         debug_print("OP_JMPGE ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPT: {
         debug_print("OP_JMPT ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPF: {
         debug_print("OP_JMPF ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPONT: {
         debug_print("OP_JMPONT ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMPONF: {
         debug_print("OP_JMPONF ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_JMP: {
         debug_print("OP_JMP ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_PUSHNILJMP: {
         debug_print("OP_PUSHNILJMP ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_FORPREP: {
         debug_print("OP_FORPREP ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_FORLOOP: {
         debug_print("OP_FORLOOP ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_LFORPREP: {
         debug_print("OP_LFORPREP ", A, " ", B, " ", U, " ", S, " ", F, "\n");

         break;
      }
      case Lua4_OP_code::OP_LFORLOOP: {
         debug_print("OP_LFORLOOP ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         break;
      }
      case Lua4_OP_code::OP_CLOSURE: {
         debug_print("OP_CLOSURE ", A, " ", B, " ", U, " ", S, " ", F, "\n");
         break;
      }

      default:
         throw std::runtime_error{"Unrecognized OP code: " + std::to_string(op)};
      }
   }

   if(state.indent == 0)
   {
       debug_print(state.buffer.str() + "\n\n");
   }
}

void create_function(Lua4_state& state)
{
   const auto name = state.stack[0];
   state.buffer << std::string(state.indent, ' ') << name << '(';
   for (int i = 1; i < state.stack.size(); ++i) {
      state.buffer << state.stack[i];
      if (i < state.stack.size() - 1) {
         state.buffer << ',';
      }
   }
   state.buffer << ")\n";
   state.stack.clear();
}
