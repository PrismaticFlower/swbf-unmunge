
#include "bit_flags.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "ucfb_reader.hpp"

#include "tbb/task_group.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

using namespace std::literals;

namespace {

struct Node_info {
   char name[16];

   float x;
   float y;
   float z;
   float radius;
};

static_assert(std::is_pod_v<Node_info>);
static_assert(sizeof(Node_info) == 32);

enum class Arc_type_flags : std::uint32_t { one_way = 1, jump = 2, jet_jump = 4 };

struct Hub_info {
   explicit Hub_info(const Node_info& node_info)
   {
      name = std::string{node_info.name,
                         cstring_length(node_info.name, sizeof(node_info.name))};
      x = node_info.x;
      y = node_info.y;
      z = node_info.z * -1.0f;
      radius = node_info.radius;
   }

   std::string name;

   float x;
   float y;
   float z;
   float radius;

   void write_to_buffer(std::string& buffer) const
   {
      buffer += "Hub(\""_sv;
      buffer += name;
      buffer += "\")\n{\n"_sv;

      buffer += "\tPos("_sv;
      buffer += std::to_string(x);
      buffer += ", "_sv;
      buffer += std::to_string(y);
      buffer += ", "_sv;
      buffer += std::to_string(z);
      buffer += ");\n"_sv;
      buffer += "\tRadius("_sv;
      buffer += std::to_string(radius);
      buffer += ");\n}\n\n"_sv;
   }
};

struct Connection_info {
   std::string name;
   std::size_t start;
   std::size_t end;
   std::uint32_t filter_flags;
   bool one_way;
   bool jump;
   bool jet_jump;

   void write_to_buffer(std::string& buffer, const std::vector<Hub_info>& hubs) const
   {
      if (start > hubs.size() || end > hubs.size()) {
         throw std::runtime_error{"Invalid planning info."};
      }

      buffer += "Connection(\""_sv;
      buffer += name;
      buffer += "\")\n{\n"_sv;

      buffer += "\tStart(\""_sv;
      buffer += hubs[start].name;
      buffer += "\");\n"_sv;
      buffer += "\tEnd(\""_sv;
      buffer += hubs[end].name;
      buffer += "\");\n"_sv;
      buffer += "\tFlags("_sv;
      buffer += std::to_string(filter_flags);
      buffer += ");\n"_sv;

      if (one_way) buffer += "\tOneWay();\n"_sv;
      if (jump) buffer += "\tJump();\n"_sv;
      if (jet_jump) buffer += "\tJetJump();\n"_sv;

      buffer += "}\n\n"_sv;
   }
};

Hub_info read_next_node(Ucfb_reader_strict<"NODE"_mn>& node, std::uint32_t hub_count)
{
   const auto info = node.read_trivial_unaligned<Node_info>();
   node.consume_unaligned(8);
   const auto weight_counts = node.read_array_unaligned<std::uint8_t>(5);

   std::uint32_t weight_count{0};

   for (const auto count : weight_counts) {
      weight_count += count;
   }

   node.consume_unaligned(weight_count * hub_count);

   return Hub_info{info};
}

Connection_info read_next_arc(Ucfb_reader_strict<"ARCS"_mn>& arcs)
{
   const auto name = arcs.read_array_unaligned<char>(16);
   const auto start = arcs.read_trivial_unaligned<std::uint8_t>();
   const auto end = arcs.read_trivial_unaligned<std::uint8_t>();
   const auto filter_flags = arcs.read_trivial_unaligned<std::uint32_t>();
   const auto type_flags = arcs.read_trivial_unaligned<Arc_type_flags>();

   Connection_info info;

   info.name = std::string{name.data(), cstring_length(name.data(), name.size())};
   info.start = start;
   info.end = end;
   info.filter_flags = filter_flags;

   info.one_way = are_flags_set(type_flags, Arc_type_flags::one_way);
   info.jump = are_flags_set(type_flags, Arc_type_flags::jump);
   info.jet_jump = are_flags_set(type_flags, Arc_type_flags::jet_jump);

   return info;
}

auto handle_node(Ucfb_reader_strict<"NODE"_mn> node, std::uint32_t hub_count)
   -> std::vector<Hub_info>
{
   std::vector<Hub_info> hubs;
   hubs.reserve(hub_count);

   while (node) {
      const auto hub = read_next_node(node, hub_count);

      hubs.emplace_back(hub);
   }

   return hubs;
}

auto handle_arcs(Ucfb_reader_strict<"ARCS"_mn> arcs, std::uint32_t arc_count)
   -> std::vector<Connection_info>
{
   std::vector<Connection_info> connections;
   connections.reserve(arc_count);

   for (std::size_t i = 0; i < arc_count; ++i) {
      const auto arc = read_next_arc(arcs);

      connections.emplace_back(arc);
   }

   return connections;
}

void write_planning(std::string name, const std::vector<Hub_info> hubs,
                    const std::vector<Connection_info> connections,
                    File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(128 * hubs.size() + connections.size());

   try {
      for (const auto& hub : hubs) {
         hub.write_to_buffer(buffer);
      }

      for (const auto& connection : connections) {
         connection.write_to_buffer(buffer, hubs);
      }
   }
   catch (std::runtime_error&) {
      buffer.clear();
      buffer += "// Failed reading planning info //"_sv;
   }

   name.append(".pln"_sv);
   file_saver.save_file(std::move(buffer), name, "world");
}
}

void handle_planning(Ucfb_reader planning, File_saver& file_saver)
{
   std::vector<Hub_info> hubs;
   std::vector<Connection_info> connections;

   auto info = planning.read_child_strict<"INFO"_mn>();

   const auto hub_count = info.read_trivial_unaligned<std::uint16_t>();
   const auto arc_count = info.read_trivial_unaligned<std::uint16_t>();

   tbb::task_group tasks;

   while (planning) {
      const auto child = planning.read_child();

      if (child.magic_number() == "NODE"_mn) {
         tasks.run([child, &hubs, hub_count] {
            hubs = handle_node(Ucfb_reader_strict<"NODE"_mn>{child}, hub_count);
         });
      }
      else if (child.magic_number() == "ARCS"_mn) {
         tasks.run([child, &connections, arc_count] {
            connections = handle_arcs(Ucfb_reader_strict<"ARCS"_mn>{child}, arc_count);
         });
      }
   }

   tasks.wait();

   static std::atomic_int plan_count{0};

   write_planning("ai_paths_"s + std::to_string(plan_count.fetch_add(1)), std::move(hubs),
                  std::move(connections), file_saver);
}
