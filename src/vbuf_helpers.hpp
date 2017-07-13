#pragma once

#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

void process_vbuf(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model);
