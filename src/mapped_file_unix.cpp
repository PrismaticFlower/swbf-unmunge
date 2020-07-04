#include "mapped_file.hpp"

#include <iostream>
#include <limits>


namespace fs = std::filesystem;


Mapped_file::Mapped_file(fs::path path)
{
   if (!fs::exists(path) || fs::is_directory(path))
      throw std::runtime_error{"File does not exist."};
   
   const auto file_size = fs::file_size(path);

   if (file_size > std::numeric_limits<std::uint32_t>::max())
      throw std::runtime_error{"File too large."};
   
   _size = static_cast<std::uint32_t>(file_size);

   file.open(path.wstring().c_str(), _size);

   if (!file.is_open())
      throw std::runtime_error{"Couldn't open file."};

   /*
   Raii_handle file = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
   */
   //if (file == INVALID_HANDLE_VALUE) throw std::invalid_argument{"File does not exist."};

   //Raii_handle file_mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);

   //if (file_mapping == NULL) throw std::runtime_error{"Unable to create file mapping."};

   /*
   const auto unmapper = [](std::byte* view) { UnmapViewOfFile(view); };

   _view = {static_cast<std::byte*>(MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0)),
            unmapper};
   */

   _view = static_cast<std::byte*>(file.data());

   if (_view == nullptr)
      throw std::runtime_error{"Shared ptr from file data failed"};
}


gsl::span<const std::byte> Mapped_file::bytes() const noexcept
{
   return {_view.get(), _size};
}