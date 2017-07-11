
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

#include "tbb/task_group.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

using namespace std::literals;

namespace {

#pragma pack(push, 1)

struct Hub {
   char name[16];

   float x;
   float y;
   float z;
   float radius;

   Byte unknown_1[8];

   std::uint8_t weight_counts[5];

   Byte weight_info[];
};

static_assert(std::is_standard_layout_v<Hub>);
static_assert(sizeof(Hub) == 45);

struct Arc {
   char name[16];
   std::uint8_t start;
   std::uint8_t end;
   std::uint32_t filter_flags;
   std::uint32_t type_flags;
};

static_assert(std::is_standard_layout_v<Arc>);
static_assert(sizeof(Arc) == 26);

struct Node {
   Magic_number mn;
   std::uint32_t size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Node>);
static_assert(sizeof(Node) == 8);

struct Arcs {
   Magic_number mn;
   std::uint32_t size;

   Arc entries[];
};

static_assert(std::is_standard_layout_v<Arcs>);
static_assert(sizeof(Arcs) == 8);

#pragma pack(pop)

struct Hub_info {
   explicit Hub_info(const Hub& hub)
   {
      this->name = hub.name;
      x = hub.x;
      y = hub.y;
      z = hub.z * -1.0f;
      radius = hub.radius;
   }

   std::string_view name;

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
   explicit Connection_info(const Arc& arc)
   {
      this->name = arc.name;
      start = arc.start;
      end = arc.end;
      filter_flags = arc.filter_flags;

      enum Type_flags { One_way = 1, Jump = 2, Jet_jump = 4 };

      one_way = ((arc.type_flags & One_way) != 0);
      jump = ((arc.type_flags & Jump) != 0);
      jet_jump = ((arc.type_flags & Jet_jump) != 0);
   }

   std::string_view name;
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

Hub_info read_hub(const Hub& hub, std::uint32_t hub_count, std::uint32_t& head)
{
   std::uint32_t weight_count{0};

   for (const auto count : hub.weight_counts) {
      weight_count += count;
   }

   head += sizeof(Hub);
   head += weight_count * hub_count;

   return Hub_info{hub};
}

auto handle_node(const Node& node, std::uint32_t hub_count) -> std::vector<Hub_info>
{
   std::uint32_t head = 0;
   const std::uint32_t end = node.size;

   std::vector<Hub_info> hubs;
   hubs.reserve(hub_count);

   while (head < end) {
      hubs.emplace_back(read_hub(view_type_as<Hub>(node.bytes[head]), hub_count, head));
   }

   return hubs;
}

auto handle_arcs(const Arcs& arcs, std::uint32_t arc_count)
   -> std::vector<Connection_info>
{
   std::vector<Connection_info> connections;
   connections.reserve(arc_count);

   for (std::size_t i = 0; i < arc_count; ++i) {
      connections.emplace_back(arcs.entries[i]);
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
   const auto& plan = planning.view_as_chunk<chunks::Planning>();

   std::uint32_t head = 0;
   const std::uint32_t end = plan.size - 16;

   std::vector<Hub_info> hubs;
   std::vector<Connection_info> connections;

   tbb::task_group tasks;

   while (head < end) {
      const auto& chunk = view_type_as<chunks::Unknown>(plan.bytes[head]);

      if (chunk.mn == "NODE"_mn) {
         tasks.run([&hubs, &chunk, hub_count{plan.info.hub_count} ] {
            hubs = handle_node(view_type_as<Node>(chunk), hub_count);
         });
      }
      else if (chunk.mn == "ARCS"_mn) {
         tasks.run([&connections, &chunk, arc_count{plan.info.arc_count} ] {
            connections = handle_arcs(view_type_as<Arcs>(chunk), arc_count);
         });
      }

      head += chunk.size + 8;
      if (head % 4 != 0) head += (4 - (head % 4));
   }

   tasks.wait();

   static std::atomic_int plan_count{0};

   write_planning("ai_paths_"s + std::to_string(plan_count.fetch_add(1)), std::move(hubs),
                  std::move(connections), file_saver);
}
