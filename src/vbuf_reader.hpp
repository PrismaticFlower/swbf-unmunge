#pragma once

#include "model_builder.hpp"
#include "ucfb_reader.hpp"
#include "vbuf_reader.hpp"

auto read_vbuf(const std::vector<Ucfb_reader_strict<"VBUF"_mn>>& vbufs,
               const std::array<glm::vec3, 2> vert_box, const bool xbox)
   -> model::Vertices;
