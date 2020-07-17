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


//TODO: FIX MESSY
#ifndef _WIN32

    ~Mapped_file(){
   	   file.close();
    }

private:
    std::uint32_t _size = 0;
    const std::byte *_view;
	boost::iostreams::mapped_file_source file;	

#else

private:
    std::uint32_t _size = 0;
    std::shared_ptr<std::byte> _view;

#endif
};