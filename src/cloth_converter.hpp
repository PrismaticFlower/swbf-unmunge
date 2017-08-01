#pragma once

#include "msh_builder.hpp"

#include <vector>

namespace msh {
// Expects the bone vector passed to it to be non-empty.
Model cloth_to_model(const Cloth& cloth, const std::vector<Bone>& bones);
}