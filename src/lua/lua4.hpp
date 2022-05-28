#pragma once

#include <cstdint>

constexpr float LUA4_TEST_NUMBER = 3.14159265358979323846E8;

using Lua4Int = std::uint32_t;
using Lua4Number = float;
using Lua4Size_t = std::uint32_t;
using Lua4Instruction = std::uint32_t;

struct Lua4_header {
   std::uint8_t endianness;             // 0 = big, 1 = little
   std::uint8_t size_int_bytes;         // size of int in bytes
   std::uint8_t size_size_t_bytes;      // size of size_t in bytes
   std::uint8_t size_instruction_bytes; // size of an instruction in bytes
   std::uint8_t size_instruction_bits;  // size of an instruction in bits
   std::uint8_t size_op_bits;           // size of an operator in bits
   std::uint8_t size_B_bits;            // size of the B register in bits
   std::uint8_t size_number_bytes;      // size of a lua_number in bytes
};

//! Declared at: https://www.lua.org/source/4.0/lopcodes.h.html
//! For 32bit systems the instruction size is 4 byte (32 bits)
//! where 6 bits make up the OP code.
enum Lua4_OP_code {
   OP_END,    // 0b000000, 0
   OP_RETURN, // 0b000001, 1

   OP_CALL, // 0b000010, 2
   OP_TAILCALL,

   OP_PUSHNIL, // 0b000100, 4
   OP_POP,

   OP_PUSHINT,
   OP_PUSHSTRING,
   OP_PUSHNUM, // 0b001000, 8
   OP_PUSHNEGNUM,

   OP_PUSHUPVALUE,

   OP_GETLOCAL,
   OP_GETGLOBAL,

   OP_GETTABLE,
   OP_GETDOTTED,
   OP_GETINDEXED,
   OP_PUSHSELF, // 0b010000, 16

   OP_CREATETABLE,

   OP_SETLOCAL,
   OP_SETGLOBAL,
   OP_SETTABLE,

   OP_SETLIST,
   OP_SETMAP,

   OP_ADD,
   OP_ADDI,
   OP_SUB,
   OP_MULT,
   OP_DIV,
   OP_POW,
   OP_CONCAT,
   OP_MINUS,
   OP_NOT,

   OP_JMPNE, // 0b100000, 32
   OP_JMPEQ,
   OP_JMPLT,
   OP_JMPLE,
   OP_JMPGT,
   OP_JMPGE,

   OP_JMPT,
   OP_JMPF,
   OP_JMPONT,
   OP_JMPONF,
   OP_JMP,

   OP_PUSHNILJMP,

   OP_FORPREP,
   OP_FORLOOP,

   OP_LFORPREP,
   OP_LFORLOOP,

   OP_CLOSURE // 0b110000, 48
};

constexpr std::uint8_t MASK_OP = 0x0000003F;  // lower 6 bits
constexpr std::uint32_t MASK_US = 0xFFFFFFC0; // upper 26 bits (U = K = N, or S = J)
constexpr std::uint32_t MASK_A = 0xFFFF8000;  // upper 17 bits (A)
constexpr std::uint32_t MASK_B = 0x00007FC0;  // middle 9 bits (B)

auto get_OP(const std::uint32_t i) -> std::uint32_t;
auto get_U(const std::uint32_t i) -> std::uint32_t;
auto get_S(const std::uint32_t i) -> std::int32_t;
auto get_A(const std::uint32_t i) -> std::uint32_t;
auto get_B(const std::uint32_t i) -> std::uint32_t;

// typedef double Number;
// typedef unsigned long Instruction;
//
// typedef struct Proto {
//    Number* knum;          /* Number numbers used by the function */
//    int nknum;             /* size of `knum' */
//    struct TString** kstr; /* strings used by the function */
//    int nkstr;             /* size of `kstr' */
//    struct Proto** kproto; /* functions defined inside the function */
//    int nkproto;           /* size of `kproto' */
//    Instruction* code;
//    int ncode; /* size of `code'; when 0 means an incomplete `Proto' */
//    short numparams;
//    short is_vararg;
//    short maxstacksize;
//    short marked;
//    struct Proto* next;
//
//    /* debug information */
//    int* lineinfo; /* map from opcodes to source lines */
//    int nlineinfo; /* size of `lineinfo' */
//    int nlocvars;
//    struct LocVar* locvars; /* information about local variables */
//    int lineDefined;
//    TString* source;
// } Proto;

