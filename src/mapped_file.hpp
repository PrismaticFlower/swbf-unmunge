#pragma once

#include "byte.hpp"

#include <gsl/gsl>

#include <cstddef>
#include <filesystem>
#include <memory>

namespace fs = std::experimental::filesystem;

class Mapped_file {
public:
   Mapped_file() = default;
   Mapped_file(fs::path path);

   gsl::span<const Byte> bytes() const noexcept;

private:
   std::shared_ptr<Byte> _view;
   std::uint32_t _size = 0;
};
