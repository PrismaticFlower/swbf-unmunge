
#include "mapped_file.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <limits>

namespace {

struct Raii_handle {
   Raii_handle(HANDLE handle) noexcept : handle(handle){};

   ~Raii_handle() noexcept
   {
      CloseHandle(handle);
   }

   operator HANDLE() noexcept
   {
      return handle;
   }

   HANDLE handle;
};
}

Mapped_file::Mapped_file(fs::path path)
{
   if (!fs::exists(path) || fs::is_directory(path)) {
      throw std::runtime_error{"File does not exist."};
   }

   const auto file_size = fs::file_size(path);

   if (file_size > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error{"File too large."};
   }

   _size = static_cast<std::uint32_t>(file_size);

   Raii_handle file = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

   if (file == INVALID_HANDLE_VALUE) throw std::invalid_argument{"File does not exist."};

   Raii_handle file_mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);

   if (file_mapping == NULL) throw std::runtime_error{"Unable to create file mapping."};

   const auto unmapper = [](Byte* view) { UnmapViewOfFile(view); };

   _view = {static_cast<Byte*>(MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0)),
            unmapper};

   if (_view == nullptr)
      throw std::runtime_error{"Unable to create view of file mapping."};
}

gsl::span<const Byte> Mapped_file::bytes() const noexcept
{
   return {_view.get(), _size};
}
