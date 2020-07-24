#include "mapped_file.hpp"

#include <limits>
#include <memory>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>

#include <iostream>


#define COUT(x) std::cout << x << std::endl;

namespace fs = std::filesystem;


Mapped_file::Mapped_file(fs::path path)
{
   if (!fs::exists(path) || fs::is_directory(path))
      throw std::runtime_error{"File does not exist"};
   
   const auto file_size = fs::file_size(path);

   if (file_size > std::numeric_limits<std::uint32_t>::max())
      throw std::runtime_error{"File too large"};

   const char *fname = path.string().c_str(); 
   _size = static_cast<std::uint32_t>(file_size);
   
   int fd = open(fname, O_RDONLY);

   if (fd < 0)
      throw std::runtime_error{std::strerror(errno)};

   std::byte *mapping = (std::byte *) mmap(0,_size,PROT_READ,MAP_FILE|MAP_PRIVATE,fd,0);
  
   if (mapping == MAP_FAILED){
      close(fd);
      throw std::runtime_error{std::strerror(errno)};
   }

   _view.reset(mapping, [this](std::byte *ptr){ munmap(ptr,_size); });
  
   close(fd);
}


gsl::span<const std::byte> Mapped_file::bytes() const noexcept
{
   return {_view.get(), _size};
}