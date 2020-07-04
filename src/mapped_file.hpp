#pragma once

#include <gsl/gsl>

#include <cstddef>
#include <filesystem>
#include <memory>

#ifdef __linux__
#include <boost/iostreams/device/mapped_file.hpp>
#endif

class Mapped_file {
public:
   Mapped_file() = default;
   Mapped_file(std::filesystem::path path);

   gsl::span<const std::byte> bytes() const noexcept;

private:
   std::shared_ptr<std::byte> _view;
   std::uint32_t _size = 0;

#ifdef __linux__
	boost::iostreams::mapped_file_source file;
#endif

};
