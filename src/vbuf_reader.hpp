#pragma once

#include "math_helpers.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#define GLM_FORCE_CXX98
#include "glm/vec3.hpp"

#include <array>

void read_vbuf(Ucfb_reader_strict<"VBUF"_mn> vbuf, msh::Model& model);