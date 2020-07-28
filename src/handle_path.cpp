
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "ucfb_reader.hpp"

#include "glm/glm.hpp"

#include <atomic>
#include <vector>

using namespace std::literals;

namespace {

struct Path_info {
   std::uint16_t node_count;
   std::uint16_t unknown_0;
   std::uint16_t unknown_1;
};

static_assert(std::is_trivially_copyable_v<Path_info>);
static_assert(sizeof(Path_info) == 6);

struct Path_node {
   glm::vec3 position;
   glm::vec4 rotation;
};

static_assert(std::is_trivially_copyable_v<Path_node>);
static_assert(sizeof(Path_node) == 28);

struct Path {
   std::string_view name;
   std::vector<std::pair<glm::vec3, glm::vec4>> nodes;
};

auto read_path_node(const Path_node& node) -> std::pair<glm::vec3, glm::vec4>
{
   std::pair<glm::vec3, glm::vec4> flipped{node.position, node.rotation};
   flipped.first.z *= -1.0f;
   flipped.second = flipped.second.zwxy();
   flipped.second.y *= -1.0f;

   return flipped;
}

Path read_path_entry(Ucfb_reader_strict<"path"_mn> entry)
{
   Path path;

   path.name = entry.read_child_strict<"NAME"_mn>().read_string();

   const auto path_info = entry.read_child_strict<"INFO"_mn>().read_trivial<Path_info>();

   path.nodes.reserve(path_info.node_count);

   while (entry) {
      auto child = entry.read_child();

      if (child.magic_number() == "PNTS"_mn) {
         const auto nodes = child.read_array<Path_node>(path_info.node_count);

         for (const auto& node : nodes) {
            path.nodes.emplace_back(read_path_node(node));
         }
      }
   }

   return path;
}

void write_node(const std::pair<glm::vec3, glm::vec4>& node, std::string& buffer)
{
   buffer += "\t\tNode()\n\t\t{\n"sv;

   const auto indent = "\t\t\t"sv;

   buffer += indent;
   buffer += "Position("sv;
   buffer += std::to_string(node.first.x);
   buffer += ", "sv;
   buffer += std::to_string(node.first.y);
   buffer += ", "sv;
   buffer += std::to_string(node.first.z);
   buffer += ");\n"sv;
   buffer += indent;
   buffer += "Rotation("sv;
   buffer += std::to_string(node.second.x);
   buffer += ", "sv;
   buffer += std::to_string(node.second.y);
   buffer += ", "sv;
   buffer += std::to_string(node.second.z);
   buffer += ", "sv;
   buffer += std::to_string(node.second.w);
   buffer += ");\n"sv;

   buffer += R"(
			Knot(0.000000);
			Data(0);
			Time(1.000000);
			PauseTime(0.000000);

			Properties(0)
			{
			}
		})"sv;

   buffer += "\n\n"sv;
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

)"sv;

   buffer += "Path(\""sv;
   buffer += path.name;
   buffer += "\")\n{\n"sv;

   buffer += path_common;
   buffer += "\tNodes("sv;
   buffer += std::to_string(path.nodes.size());
   buffer += ")\n\t{\n";

   for (const auto& node : path.nodes) {
      write_node(node, buffer);
   }

   buffer += "\t}\n}\n\n"sv;
}

void save_paths(std::vector<Path> paths, File_saver& file_saver)
{
   std::string buffer;
   buffer.reserve(2048 * paths.size());

   buffer += "Version(10);\n"sv;
   buffer += "PathCount("sv;
   buffer += std::to_string(paths.size());
   buffer += ");\n\n"sv;

   for (const auto& path : paths) {
      write_path(path, buffer);
   }

   static std::atomic_int path_count{0};

   std::string file_name = std::to_string(path_count.fetch_add(1));

   file_saver.save_file(buffer, "world"s, file_name, ".pth"sv);
}
}

void handle_path(Ucfb_reader path, File_saver& file_saver)
{
   std::vector<Path> paths;

   while (path) {
      const auto child = path.read_child_strict<"path"_mn>();

      paths.emplace_back(read_path_entry(child));
   }

   save_paths(std::move(paths), file_saver);
}