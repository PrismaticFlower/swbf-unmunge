#pragma once

#include "byte.hpp"

#include <filesystem>
#include <memory>

namespace fs = std::experimental::filesystem;

class Mapped_file {
public:
   Mapped_file() = default;
   Mapped_file(fs::path path);

   const Byte* get_bytes() noexcept;

private:
   std::shared_ptr<Byte> _view;
};
