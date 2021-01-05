#pragma once

#include <array>
#include <type_traits>

template<typename Type>
constexpr Type range_convert(Type value,
                             std::array<std::type_identity_t<Type>, 2> old_range,
                             std::array<std::type_identity_t<Type>, 2> new_range)
{
   const Type old_range_value = (old_range[0] - old_range[1]);
   const Type new_range_value = (new_range[0] - new_range[1]);

   return (((value - old_range[0]) * new_range_value) / old_range_value) + new_range[0];
}