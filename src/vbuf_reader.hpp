#pragma once

#include "math_helpers.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

void read_vbuf(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model);

void read_vbuf_xbox(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model,
                    const std::array<glm::vec3, 2> vert_box);
