#pragma once

#include <gsl/gsl>
#include <fmt/format.h>
#include <sstream>
#include <iomanip>

//! \brief  Creates a string containing the hex and text representation of the
//!         buffer.
//!
//! \tparam Type    The type of the span buffer.
//!
//! \param  buffer  A span buffer of type Type to dump.
//!
//! \param  offset  Defines the offset in bytes after which the hex table or
//!                 the text is created.
//!
//! \return A const std::string.
template<typename Type>
std::string dump(const Type* buffer, const unsigned len, const unsigned char offset = 0)
{
   unsigned char row_len = 16;
   unsigned char control_char = '.';

   std::stringstream ss;
   size_t pos = 0;
   unsigned char sym_offset = 0;
   unsigned char addr_offset = 0;
   const unsigned char* hex = reinterpret_cast<const unsigned char*>(buffer);
   const unsigned char* sym = hex;

   // Print address todo

   // Add the offset to hex table (spaces)
   ss << '\n';
   for (; pos < offset; ++pos) ss << "   ";

   // Print the hex table and the symbols
   ss << std::hex << std::setfill('0');
   while (pos < len) {

      // Print symbols after hex digits
      if (pos % row_len == 0) {
         ss << "  ";

         // Add offset to symbols
         while (sym_offset++ < offset) ss << ' ';

         // Print character symbols of this line
         while (sym < hex) {
            if (*sym >= 0x20 && *sym <= 0x7E)
               ss << *sym;
            else
               ss << control_char;
            ++sym;
         }
         ss << '\n';

         // Print address todo
      }

      // Print the hex digits of the byte.
      ss << std::setw(2) << static_cast<unsigned>(*hex) << ' ';
      ++hex;
      ++pos;
   }

   ss << '\n';
   return ss.str();
}