
#include"chunk_headers.hpp"
#include"file_saver.hpp"
#include"magic_number.hpp"
#include"string_helpers.hpp"
#include"type_pun.hpp"

#define GLM_FORCE_CXX98
#define GLM_FORCE_SWIZZLE

#include"glm/vec3.hpp"
#include"glm/vec4.hpp"

#include<atomic>
#include<vector>

using namespace std::literals;

namespace
{

#pragma pack(push, 1)

struct Path_entry
{
   Magic_number mn;
   std::uint32_t size;
   std::uint32_t name_mn;
   std::uint32_t name_size;

   Byte bytes[];
};

static_assert(std::is_standard_layout_v<Path_entry>);
static_assert(sizeof(Path_entry) == 16);

struct Path_info
{
   Magic_number mn;
   std::uint32_t size;
   std::uint16_t node_count;
   std::uint16_t unknown_0;
   std::uint16_t unknown_1;
};

static_assert(std::is_standard_layout_v<Path_info>);
static_assert(sizeof(Path_info) == 14);

struct Path_node
{
   glm::vec3 position;
   glm::vec4 rotation;
};

static_assert(std::is_standard_layout_v<Path_node>);
static_assert(sizeof(Path_node) == 28);

struct Path_points
{
   Magic_number mn;
   std::uint32_t size;

   Path_node nodes[];
};

static_assert(std::is_standard_layout_v<Path_points>);
static_assert(sizeof(Path_points) == 8);

#pragma pack(pop)

struct Path
{
   std::string_view name;
   std::vector<Path_node> nodes;
};

Path_node flip_path_node(const Path_node& node)
{
   Path_node flipped{node};
   flipped.position.z *= -1.0f;
   flipped.rotation = node.rotation.zwxy();
   flipped.rotation.y *= -1.0f;

   return flipped;
}

Path read_path_entry(const Path_entry& entry)
{
   Path path;

   std::uint32_t head = 0;
   const std::uint32_t end = entry.size - 8;

   const auto align_head = [&head] { if (head % 4 != 0) head += (4 - (head % 4)); };

   path.name = {reinterpret_cast<const char*>(&entry.bytes[head]), entry.name_size - 1};

   head += entry.name_size;
   align_head();
   
   const auto& path_info = view_type_as<Path_info>(entry.bytes[head]);

   head += path_info.size + 8;
   align_head();

   path.nodes.reserve(path_info.node_count);

   while (head < end) {
      const auto& child = view_type_as<chunks::Unknown>(entry.bytes[head]);

      if (child.mn == "PNTS"_mn) {
         const auto& path_points = view_type_as<Path_points>(entry.bytes[head]);

         for (std::size_t i = 0; i < path_info.node_count; ++i) {
            path.nodes.emplace_back(flip_path_node(path_points.nodes[i]));
         }
      }

      head += child.size + 8;
      align_head();
   }

   return path; 
}

void write_node(const Path_node& node, std::string& buffer)
{
   buffer += "\t\tNode()\n\t\t{\n"_sv;

   const auto indent = "\t\t\t"_sv;

   buffer += indent;
   buffer += "Position("_sv;
   buffer += std::to_string(node.position.x); buffer += ", "_sv;
   buffer += std::to_string(node.position.y); buffer += ", "_sv;
   buffer += std::to_string(node.position.z); buffer += ");\n"_sv;
   buffer += indent;
   buffer += "Rotation("_sv;
   buffer += std::to_string(node.rotation.x); buffer += ", "_sv;
   buffer += std::to_string(node.rotation.y); buffer += ", "_sv;
   buffer += std::to_string(node.rotation.z); buffer += ", "_sv;
   buffer += std::to_string(node.rotation.w); buffer += ");\n"_sv;

   buffer += R"(
			Knot(0.000000);
			Data(0);
			Time(1.000000);
			PauseTime(0.000000);

			Properties(0)
			{
			}
		})"_sv;

   buffer += "\n\n"_sv;
}

void write_path(const Path& path, std::string& buffer) 
{
   const auto path_common = R"(	Data(0);
	PathType(0);
	PathSpeedType(0);
	PathTime(0.000000);
	OffsetPath(0);
	SplineType("Hermite");

	Properties(0)
	{
	}

)"_sv;

   buffer += "Path(\""_sv;
   buffer += path.name;
   buffer += "\")\n{\n"_sv;

   buffer += path_common;
   buffer += "\tNodes("_sv;
   buffer += std::to_string(path.nodes.size());
   buffer += ")\n\t{\n";

   for (const auto& node : path.nodes) {
      write_node(node, buffer);
   }

}

void save_paths(std::vector<Path> paths, File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(2048 * paths.size());

   buffer += "Version(10);\n"_sv;
   buffer += "PathCount("_sv;
   buffer += std::to_string(paths.size());
   buffer += ");\n\n"_sv;

   for (const auto& path : paths) {
      write_path(path, buffer);
   }

   static std::atomic_int path_count{0};

   std::string file_name = std::to_string(path_count.fetch_add(1));
   file_name += ".pth"_sv;

   file_saver.save_file(std::move(buffer), std::move(file_name), "world"s);
}

}

void handle_path(const chunks::Path& path,
                 File_saver& file_saver)
{
   std::uint32_t head = 0;
   const std::uint32_t end = path.size;

   std::vector<Path> paths;

   while (head < end) {
      const auto& entry = view_type_as<Path_entry>(path.bytes[head]);

      if (entry.mn == "path"_mn) {
         paths.emplace_back(read_path_entry(entry));
      }

      head += entry.size + 8;
      if (head % 4 != 0) head += (4 - (head % 4));
   }

   save_paths(std::move(paths), file_saver);
}