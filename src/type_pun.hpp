#pragma once

#include <gsl/gsl>

#include <string>
#include <type_traits>

template<typename Type, typename Actual>
inline const Type& view_type_as(const Actual& what) noexcept
{
   static_assert(std::is_standard_layout_v<Actual>,
                 "Actual type must be standard layout.");
   static_assert(std::is_standard_layout_v<Type>, "Viewed type must be standard layout.");
   static_assert(!std::is_reference_v<Type>,
                 "Viewed type will be a reference automatically.");
   static_assert(!std::is_pointer_v<Type>, "Viewed type can not be a pointer.");
   static_assert(!std::is_const_v<Type>,
                 "Viewed type will have const added automatically.");

   return reinterpret_cast<const Type&>(what);
}

template<typename Pod>
inline std::string_view view_pod_as_string(const Pod& pod)
{
   static_assert(std::is_pod_v<std::remove_reference_t<Pod>>,
                 "Type must be plain-old-data.");
   static_assert(!std::is_pointer_v<std::remove_reference_t<Pod>>,
                 "Type can not be a pointer.");

   return {reinterpret_cast<const char*>(&pod), sizeof(Pod)};
}

template<typename Pod>
inline std::string view_pod_as_string(Pod&& pod)
{
   static_assert(std::is_pod_v<std::remove_reference_t<Pod>>,
                 "Type must be plain-old-data.");
   static_assert(!std::is_pointer_v<std::remove_reference_t<Pod>>,
                 "Type can not be rvalue to pointer.");

   return {reinterpret_cast<const char*>(&pod), sizeof(Pod)};
}

template<typename Pod>
inline std::string_view view_pod_span_as_string(gsl::span<const Pod> array)
{
   static_assert(std::is_pod_v<std::remove_reference_t<Pod>>,
                 "Type must be plain-old-data.");
   static_assert(!std::is_pointer_v<std::remove_reference_t<Pod>>,
                 "Type can not be a pointer.");

   return {reinterpret_cast<const char*>(array.data()),
           static_cast<std::size_t>(array.size_bytes())};
}
