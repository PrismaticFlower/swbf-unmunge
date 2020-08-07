#pragma once
#include <stdint.h>

/*

Cross platform integer literals.  Clang/GCC demand the underscore for 
integer literals.

*/

constexpr int16_t operator "" _i16(unsigned long long int lit){return ((int16_t) lit);}
constexpr int32_t operator "" _i32(unsigned long long int lit){return ((int32_t) lit);}

constexpr uint16_t operator "" _ui16(unsigned long long int lit){return ((uint16_t) lit);}
constexpr uint32_t operator "" _ui32(unsigned long long int lit){return ((uint32_t) lit);}