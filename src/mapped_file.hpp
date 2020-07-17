#pragma once

#include <gsl/gsl>

#include <cstddef>
#include <filesystem>
#include <memory>

#ifndef _WIN32 
#include <boost/iostreams/device/mapped_file.hpp>
#endif


class Mapped_file {
public:
   Mapped_file() = default;
   Mapped_file(std::filesystem::path path);

   gsl::span<const std::byte> bytes() const noexcept;

   ~Mapped_file(){
   	   file.close();
   }

private:
   const std::byte *_view;
   std::uint32_t _size = 0;

#ifndef _WIN32
	boost::iostreams::mapped_file_source file;
#endif

};