#pragma once

#include "model_types.hpp"

namespace model {

auto convert_topology(const Indices& indices, const Primitive_topology current,
                      const Primitive_topology desired) -> Indices;

}