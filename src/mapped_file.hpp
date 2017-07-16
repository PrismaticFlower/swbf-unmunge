#pragma once

#include "byte.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>

namespace fs = std::experimental::filesystem;

class Mapped_file {
public:
   Mapped_file() = default;
   Mapped_file(fs::path path);

   const Byte* bytes() noexcept;

   std::uint32_t size() noexcept;

private:
   std::shared_ptr<Byte> _view;
   std::uint32_t _size = 0;
};
