
#include "model_topology_converter.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <stdexcept>

#include <fmt/format.h>

namespace model {

namespace {

template<typename Type>
constexpr bool is_degenerate_triangle(const std::array<Type, 3> triangle) noexcept
{
   return triangle[0] == triangle[1] || triangle[0] == triangle[2] ||
          triangle[1] == triangle[2];
}

template<typename Type>
constexpr bool is_even(Type size)
{
   return (size % 2) == 0;
}

template<typename Type>
constexpr bool is_odd(Type size)
{
   return !is_even(size);
}

auto count_strips(const std::vector<Indices>& strips) noexcept -> std::size_t
{
   return std::accumulate(
      strips.cbegin(), strips.cend(), std::size_t{0},
      [](const std::size_t size, const Indices& indices) noexcept {
         return size + indices.size();
      });
}

auto combine_triangle_strips(const std::vector<Indices>& strips) -> Indices
{
   [[unlikely]] if (strips.size() == 0) return {};

   const auto padding_count = (std::max(strips.size() - 1, std::size_t{1}) * 2);

   Indices indices;
   indices.reserve(count_strips(strips) + padding_count);

   indices.insert(indices.end(), strips[0].cbegin(), strips[0].cend());
   indices.push_back(strips[0].back());

   std::for_each(strips.cbegin() + 1, strips.cend(), [&](const Indices& strip) {
      if (strip.size() < 3) return;

      indices.push_back(strip.front());
      indices.insert(indices.end(), strip.cbegin(), strip.cend());
      indices.push_back(strip.back());
   });

   if (indices.size() != (strips[0].size() + 1)) indices.pop_back();

   return indices;
}

auto combine_triangle_strips_ps2(const std::vector<Indices>& strips) -> Indices
{
   [[unlikely]] if (strips.size() == 0) return {};

   Indices indices;
   indices.reserve(count_strips(strips));

   std::for_each(strips.cbegin() + 1, strips.cend(), [&](const Indices& strip) {
      if (strip.size() < 3) return;

      indices.push_back(strip[0] | 0x8000);
      indices.push_back(strip[1] | 0x8000);
      indices.insert(indices.end(), strip.cbegin() + 2, strip.cend());
   });

   return indices;
}

auto create_triangle_strips(const Indices& tris) -> std::vector<Indices>
{
   [[unlikely]] if (tris.size() < 3) return {tris};

   std::vector<Indices> strips;

   for (std::size_t i = 2; i < tris.size(); i += 3) {
      const std::array cw_tri{tris[i - 2], tris[i - 1], tris[i]};
      const std::array ccw_tri{cw_tri[2], cw_tri[1], cw_tri[0]};

      bool inserted = false;
      for (auto& strip : strips) {
         if (strip.size() < 3) continue;

         const auto tri = is_even(strip.size() / 3) ? cw_tri : ccw_tri;
         const std::array strip_head{*(strip.end() - 2), *(strip.end() - 1)};

         if (std::array{tri[0], tri[1]} == strip_head) {
            strips.emplace_back(tri[2]);
            inserted = true;
            break;
         }
      }

      if (!inserted) strips.emplace_back(cw_tri.cbegin(), cw_tri.cend());
   }

   return strips;
}

template<Primitive_topology current, Primitive_topology desired>
auto convert([[maybe_unused]] const Indices& indices) -> Indices
{
   throw std::runtime_error{fmt::format(
      "Attempt to convert primitve topology from {} to {}. This is unsupported.",
      to_string_view(current), to_string_view(desired))};
}

template<>
auto convert<Primitive_topology::triangle_list, Primitive_topology::triangle_strip>(
   const Indices& tris) -> Indices
{
   return combine_triangle_strips(create_triangle_strips(tris));
}

template<>
auto convert<Primitive_topology::triangle_list, Primitive_topology::triangle_strip_ps2>(
   const Indices& tris) -> Indices
{
   return combine_triangle_strips_ps2(create_triangle_strips(tris));
}

template<>
auto convert<Primitive_topology::triangle_strip_ps2, Primitive_topology::triangle_list>(
   const Indices& strips) -> Indices
{
   [[unlikely]] if (strips.size() < 3) return {};

   Indices triangles{};
   triangles.reserve(strips.size() * 3 - 2);

   for (std::size_t i = 2; i < strips.size();) {
      if ((strips[i] & 0x7fff) && ((i + 2) >= strips.size())) {
         if (!(strips[i + 1] & 0x7fff)) continue;

         i += 2;
      }

      auto tri = is_even(i / 3) ? std::array{strips[i - 2], strips[i - 1], strips[i]}
                                : std::array{strips[i], strips[i - 1], strips[i - 2]};

      if (is_degenerate_triangle(tri)) continue;

      for (auto& index : tri) index &= 0x7fff;

      triangles.insert(triangles.end(), tri.cbegin(), tri.cend());

      i += 1;
   }

   return triangles;
}

template<>
auto convert<Primitive_topology::triangle_strip_ps2, Primitive_topology::triangle_strip>(
   const Indices& ps2_strips) -> Indices
{
   [[unlikely]] if (ps2_strips.size() < 3) return {};

   Indices new_strips{};
   new_strips.reserve(ps2_strips.size() + (ps2_strips.size() / 4));
   new_strips.insert(new_strips.end(), ps2_strips.begin(), ps2_strips.begin() + 3);

   for (std::size_t i = 3; i < ps2_strips.size(); ++i) {
      const auto index = static_cast<std::uint16_t>(ps2_strips[i] & 0x7fff);

      if ((ps2_strips[i] & 0x7fff) && ((i + 1) >= ps2_strips.size()) &&
          (ps2_strips[i + 1] & 0x7fff)) {
         new_strips.push_back(std::uint16_t{new_strips.back()});
         new_strips.push_back(index);
      }

      new_strips.push_back(index);
   }

   return new_strips;
}

template<>
auto convert<Primitive_topology::triangle_strip, Primitive_topology::triangle_list>(
   const Indices& strips) -> Indices
{
   [[unlikely]] if (strips.size() < 3) return {};

   Indices triangles{};
   triangles.reserve(strips.size() * 3 - 2);

   for (std::size_t i = 2; i < strips.size(); ++i) {
      const auto tri = is_even(i / 3)
                          ? std::array{strips[i - 2], strips[i - 1], strips[i]}
                          : std::array{strips[i], strips[i - 1], strips[i - 2]};

      if (is_degenerate_triangle(tri)) continue;

      triangles.insert(triangles.end(), tri.cbegin(), tri.cend());
   }

   return triangles;
}

template<>
auto convert<Primitive_topology::triangle_strip, Primitive_topology::triangle_strip_ps2>(
   const Indices& strips) -> Indices
{
   [[unlikely]] if (strips.size() < 3) return {};

   Indices ps2_strips{};
   ps2_strips.reserve(ps2_strips.size());
   ps2_strips.insert(ps2_strips.end(), strips.begin(), strips.begin() + 3);

   ps2_strips[0] |= 0x8000;
   ps2_strips[1] |= 0x8000;

   for (std::size_t i = 3; i < strips.size();) {
      if ((strips[i] == ps2_strips.back()) && (i + 3 >= strips.size()) &&
          (strips[i + 1] != strips[i + 2])) {

         ps2_strips.push_back(strips[i + 2] | 0x8000);
         ps2_strips.push_back(strips[i + 3] | 0x8000);

         i += 3;
         continue;
      }
      else {
         ps2_strips.push_back(strips[i]);
         i += 1;
      }
   }

   return ps2_strips;
}

template<>
auto convert<Primitive_topology::triangle_fan, Primitive_topology::triangle_list>(
   const Indices& fan) -> Indices
{
   [[unlikely]] if (fan.size() < 3) return {};

   Indices triangles{};
   triangles.reserve(fan.size() * 3 - 2);

   for (std::size_t i = 2; i < fan.size(); ++i) {
      triangles.insert(triangles.end(), {fan[0], fan[i - 1], fan[i]});
   }

   return triangles;
}

template<>
auto convert<Primitive_topology::triangle_fan, Primitive_topology::triangle_strip>(
   const Indices& fan) -> Indices
{
   return convert<Primitive_topology::triangle_list, Primitive_topology::triangle_strip>(
      convert<Primitive_topology::triangle_fan, Primitive_topology::triangle_list>(fan));
}

template<>
auto convert<Primitive_topology::triangle_fan, Primitive_topology::triangle_strip_ps2>(
   const Indices& fan) -> Indices
{
   return convert<Primitive_topology::triangle_list,
                  Primitive_topology::triangle_strip_ps2>(
      convert<Primitive_topology::triangle_fan, Primitive_topology::triangle_list>(fan));
}

template<Primitive_topology current>
auto dispatch(const Indices& indices, const Primitive_topology desired) -> Indices
{

   switch (desired) {
   case Primitive_topology::point_list:
      return convert<current, Primitive_topology::point_list>(indices);
   case Primitive_topology::line_list:
      return convert<current, Primitive_topology::line_list>(indices);
   case Primitive_topology::line_loop:
      return convert<current, Primitive_topology::line_loop>(indices);
   case Primitive_topology::line_strip:
      return convert<current, Primitive_topology::line_strip>(indices);
   case Primitive_topology::triangle_list:
      return convert<current, Primitive_topology::triangle_list>(indices);
   case Primitive_topology::triangle_strip:
      return convert<current, Primitive_topology::triangle_strip>(indices);
   case Primitive_topology::triangle_strip_ps2:
      return convert<current, Primitive_topology::triangle_strip_ps2>(indices);
   case Primitive_topology::triangle_fan:
      return convert<current, Primitive_topology::triangle_fan>(indices);
   case Primitive_topology::undefined:
   default:
      throw std::runtime_error{"attempt to convert to undefined primitve topology"};
   }
}
}

auto convert_topology(const Indices& indices, const Primitive_topology current,
                      const Primitive_topology desired) -> Indices
{
   if (current == desired) return indices;

   switch (current) {
   case Primitive_topology::point_list:
      return dispatch<Primitive_topology::point_list>(indices, desired);
   case Primitive_topology::line_list:
      return dispatch<Primitive_topology::line_list>(indices, desired);
   case Primitive_topology::line_loop:
      return dispatch<Primitive_topology::line_loop>(indices, desired);
   case Primitive_topology::line_strip:
      return dispatch<Primitive_topology::line_strip>(indices, desired);
   case Primitive_topology::triangle_list:
      return dispatch<Primitive_topology::triangle_list>(indices, desired);
   case Primitive_topology::triangle_strip:
      return dispatch<Primitive_topology::triangle_strip>(indices, desired);
   case Primitive_topology::triangle_strip_ps2:
      return dispatch<Primitive_topology::triangle_strip_ps2>(indices, desired);
   case Primitive_topology::triangle_fan:
      return dispatch<Primitive_topology::triangle_fan>(indices, desired);
   case Primitive_topology::undefined:
   default:
      throw std::runtime_error{"attempt to convert from undefined primitve topology"};
   }
}
}