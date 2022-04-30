#pragma once

#include "lua/ILuaDecompiler.hpp"

class Lua4Decompiler final : public ILuaDecompiler {
public:
   explicit Lua4Decompiler();
   virtual ~Lua4Decompiler() override = default;

   virtual bool readHeader() override;
};