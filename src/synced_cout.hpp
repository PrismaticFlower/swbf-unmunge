#pragma once

#include <iostream>
#include <mutex>
#include <utility>

namespace synced_cout {

namespace detail {
inline std::mutex& cout_mutex() noexcept
{
   static std::mutex mutex;

   return mutex;
}

template<typename Arg>
inline void write(Arg&& arg)
{
   std::cout << std::forward<Arg>(arg);
}
}

template<typename... Args>
inline void print(Args&&... args)
{
   std::lock_guard<std::mutex> lock{detail::cout_mutex()};

   [[maybe_unused]] const bool dummy_list[] = {
      (detail::write(std::forward<Args>(args)), false)...};
}
}