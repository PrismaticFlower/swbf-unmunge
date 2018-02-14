#pragma once

#include "math_helpers.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

void read_vbuf(const std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs, msh::Model& model,
               bool* const pretransformed);

void read_vbuf_xbox(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model,
                    const std::array<glm::vec3, 2> vert_box, bool* const pretransformed);
