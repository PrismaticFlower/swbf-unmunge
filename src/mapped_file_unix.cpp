#include "mapped_file.hpp"

#include <iostream>
#include <limits>

#define COUT(x) std::cout << x << std::endl;

namespace fs = std::filesystem;


Mapped_file::Mapped_file(fs::path path)
{
   if (!fs::exists(path) || fs::is_directory(path))
      throw std::runtime_error{"File does not exist."};
   
   const auto file_size = fs::file_size(path);

   if (file_size > std::numeric_limits<std::uint32_t>::max())
      throw std::runtime_error{"File too large."};
   
   _size = static_cast<std::uint32_t>(file_size);

   COUT("About to open")
   COUT(path.string())

   boost::iostreams::mapped_file_params parameters;
   parameters.path = path.string();
   parameters.length = static_cast<size_t>(file_size);
   parameters.flags = boost::iostreams::mapped_file::mapmode::readonly;
   parameters.offset = static_cast<boost::iostreams::stream_offset>(0);

   file.open(parameters);

   if (!file.is_open())
      throw std::runtime_error{"Couldn't open file."};

   COUT("Opened")

   _view = reinterpret_cast<const std::byte *>(file.data());

   if (_view == nullptr)
      throw std::runtime_error{"Ptr from file data failed"};
}


gsl::span<const std::byte> Mapped_file::bytes() const noexcept
{
   return {_view, _size};
}