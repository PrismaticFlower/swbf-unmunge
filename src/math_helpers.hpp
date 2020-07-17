#pragma once

#include <array>

template<typename Type>
constexpr Type range_convert(Type value, std::array<Type, 2> old_range,
                             std::array<Type, 2> new_range)
{
   const Type old_range_value = (old_range[0] - old_range[1]);
   const Type new_range_value = (new_range[0] - new_range[1]);

   return (((value - old_range[0]) * new_range_value) / old_range_value) + new_range[0];
}