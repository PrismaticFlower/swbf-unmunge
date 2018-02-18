#pragma once

#include <gsl/gsl>

#include <cstddef>
#include <filesystem>
#include <memory>

namespace fs = std::experimental::filesystem;

class Mapped_file {
public:
   Mapped_file() = default;
   Mapped_file(fs::path path);

   gsl::span<const std::byte> bytes() const noexcept;

private:
   std::shared_ptr<std::byte> _view;
   std::uint32_t _size = 0;
};
