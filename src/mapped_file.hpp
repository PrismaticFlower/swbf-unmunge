#pragma once

#include <gsl/gsl>

#include <cstddef>
#include <filesystem>
#include <memory>

class Mapped_file {

public:
    Mapped_file() = default;
    Mapped_file(std::filesystem::path path);

    gsl::span<const std::byte> bytes() const noexcept;

private:
    std::uint32_t _size = 0;
    std::shared_ptr<std::byte> _view;
};