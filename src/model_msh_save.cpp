
#include "model_msh_save.hpp"
#include "file_saver.hpp"
#include "model_topology_converter.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"
#include "ucfb_writer.hpp"

#include <iterator>
#include <sstream>

#include <fmt/format.h>
#include <glm/gtc/quaternion.hpp>

using namespace std::literals;

namespace model::msh {

namespace {

enum class Model_type : std::uint32_t {
   null = 0,
   skin = 1,
   cloth = 2,
   bone = 3,
   fixed = 4
};

enum class Model_flags : std::uint32_t {
   none = 0b0,
   hidden = 0b1,
};

auto make_model_name_lookup_table(const std::vector<scene::Node>& nodes)
   -> std::vector<std::string>
{
   std::vector<std::string> lut;
   lut.reserve(nodes.size());

   std::transform(nodes.cbegin(), nodes.cend(), std::back_inserter(lut),
                  [](const scene::Node& node) { return node.name; });

   return lut;
}

void sort_nodes(std::vector<scene::Node>& nodes)
{
   std::vector<scene::Node> sorted;
   sorted.reserve(nodes.size());

   // find root
   {
      auto root = std::find_if(nodes.begin(), nodes.end(), [](const scene::Node& node) {
         return node.parent.empty();
      });

      if (root == nodes.end()) {
         throw std::runtime_error{"unable to find root node in model scene"};
      }

      sorted.emplace_back(std::move(*root));
      nodes.erase(root);
   }

   auto move_in_kids = [&](auto this_func, const std::string_view parent) noexcept->void
   {
      std::vector<scene::Node>::iterator it;
      while ((it = std::find_if(nodes.begin(), nodes.end(), [&](const scene::Node& node) {
                 return parent == node.parent;
              })) != nodes.end()) {
         sorted.emplace_back(std::move(*it));
         nodes.erase(it);

         this_func(this_func, sorted.back().name);
      }
   };

   move_in_kids(move_in_kids, sorted.back().name);

   if (!nodes.empty()) {
      throw std::runtime_error{"model scene has ophaned nodes"};
   }

   std::swap(sorted, nodes);
}

void patch_bone_maps(std::vector<scene::Node>& nodes,
                     const std::vector<std::string>& previous_names_lut)
{
   for (auto& node : nodes) {
      if (!node.geometry) continue;

      for (auto& index : node.geometry->bone_map) {
         auto it =
            std::find_if(nodes.cbegin(), nodes.cend(),
                         [name = previous_names_lut.at(index)](const scene::Node& node) {
                            return node.name == name;
                         });

         index = static_cast<std::uint8_t>(std::distance(nodes.cbegin(), it));
      }
   }
}

auto get_model_type(const scene::Node& node) noexcept -> Model_type
{
   switch (node.type) {
   case scene::Node_type::null:
      return (begins_with(node.name, "bone"sv)) ? Model_type::bone : Model_type::null;
   case scene::Node_type::geometry:
      if (!node.geometry) return Model_type::null;

      return (node.geometry->bone_map.empty()) ? Model_type::fixed : Model_type::skin;
   case scene::Node_type::cloth_geometry:
      return Model_type::cloth;
   case scene::Node_type::collision:
   case scene::Node_type::collision_primitive:
      return Model_type::fixed;
   default:
      std::terminate();
   }
}

bool is_hidden(const scene::Node& node) noexcept
{
   switch (node.type) {
   case scene::Node_type::null:
   case scene::Node_type::collision:
   case scene::Node_type::collision_primitive:
      return true;
   case scene::Node_type::geometry:
   case scene::Node_type::cloth_geometry:
      return node.lod != Lod::zero;
   default:
      std::terminate();
   }
}

void write_bbox(Ucfb_writer& parent, const scene::AABB aabb)
{
   const auto centre = (aabb.max + aabb.min) / 2.0f;
   const auto size = (aabb.max - aabb.min) / 2.0f;

   parent.emplace_child("BBOX"_mn).write(glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}, centre, size,
                                         glm::length(size));
}

void write_sinf(Ucfb_writer& msh2, const scene::Scene& scene)
{
   auto sinf = msh2.emplace_child("SINF"_mn);

   sinf.emplace_child("NAME"_mn).write(scene.name);
   sinf.emplace_child("FRAM"_mn).write<int, int, float>(0, 1, 29.97003f);
   write_bbox(sinf, scene.aabb);
}

void write_matd(Ucfb_writer& matl, const scene::Material& material)
{
   auto matd = matl.emplace_child("MATD"_mn);

   matd.emplace_child("NAME"_mn).write(material.name);
   matd.emplace_child("DATA"_mn).write(material.diffuse_colour, material.specular_colour,
                                       glm::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                                       material.specular_exponent);
   matd.emplace_child("ATRB"_mn).write_unaligned(material.flags, material.rendertype,
                                                 material.params);

   constexpr std::array tx_d_magic_numbers{"TX0D"_mn, "TX1D"_mn, "TX2D"_mn, "TX3D"_mn};

   for (auto i = 0; i < material.textures.size(); ++i) {
      if (material.textures[i].empty()) continue;

      matd.emplace_child(tx_d_magic_numbers.at(i))
         .write(fmt::format("{}.tga"sv, material.textures[i]));
   }
}

void write_matl(Ucfb_writer& msh2, const scene::Scene& scene)
{
   auto matl = msh2.emplace_child("MATL"_mn);

   matl.write(static_cast<std::uint32_t>(scene.materials.size()));

   for (const auto& material : scene.materials) {
      write_matd(matl, material);
   }
}

void write_tran(Ucfb_writer& modl, const glm::mat4x3 transform)
{
   const glm::vec3 translation = transform[3];
   const glm::vec3 scale = {glm::length(transform[0]), glm::length(transform[1]),
                            glm::length(transform[2])};
   const glm::mat3 rotation_mat{transform[0] / scale.x, transform[1] / scale.y,
                                transform[2] / scale.z};
   const glm::quat rotation{rotation_mat};

   modl.emplace_child("TRAN"_mn).write(scale, rotation, translation);
}

void write_wght(Ucfb_writer& segm, const Vertices& vertices)
{
   Expects(vertices.bones);

   auto wght = segm.emplace_child("WGHT"_mn);

   wght.write(static_cast<std::uint32_t>(vertices.size));

   if (vertices.weights) {
      for (std::size_t i = 0; i < vertices.size; ++i) {
         wght.write(std::uint32_t{vertices.bones[i].x}, vertices.weights[i].x,
                    std::uint32_t{vertices.bones[i].y}, vertices.weights[i].y,
                    std::uint32_t{vertices.bones[i].z}, vertices.weights[i].z,
                    std::uint32_t{0}, 0.0f);
      }
   }
   else {
      for (std::size_t i = 0; i < vertices.size; ++i) {
         wght.write(std::uint32_t{vertices.bones[i].x}, 1.0f, std::uint32_t{}, 0.0f,
                    std::uint32_t{}, 0.0f, std::uint32_t{}, 0.0f);
      }
   }
}

void write_clrl(Ucfb_writer& segm, const gsl::span<glm::vec4> colours)
{

   auto clrl = segm.emplace_child("CLRL"_mn);

   clrl.write(static_cast<std::uint32_t>(colours.size()));

   for (const auto c : colours) clrl.write<std::uint32_t>(glm::packUnorm4x8(c.bgra));
}

void write_ndxl_ndxt(Ucfb_writer& segm, const scene::Geometry& geometry)
{
   std::optional<Indices> converted;
   const auto indices = [&]() -> const Indices& {
      if (geometry.topology != Primitive_topology::triangle_list) {
         return *(converted = convert_topology(geometry.indices, geometry.topology,
                                               Primitive_topology::triangle_list));
      }

      return geometry.indices;
   }();

   // NDXL
   {
      auto ndxl = segm.emplace_child("NDXL"_mn);

      ndxl.write(static_cast<std::uint32_t>(indices.size() / 3));

      for (auto i = 2; i < indices.size(); i += 3) {
         ndxl.write(std::uint16_t{3}, indices[i - 2], indices[i - 1], indices[i]);
      }
   }

   // NDXT
   segm.emplace_child("NDXT"_mn).write(static_cast<std::uint32_t>(indices.size() / 3),
                                       gsl::make_span(indices));
}

void write_strp(Ucfb_writer& segm, const scene::Geometry& geometry)
{
   std::optional<Indices> converted;
   const Indices& indices = [&]() -> const Indices& {
      if (geometry.topology != Primitive_topology::triangle_strip_ps2) {
         return *(converted = convert_topology(geometry.indices, geometry.topology,
                                               Primitive_topology::triangle_strip_ps2));
      }

      return geometry.indices;
   }();

   segm.emplace_child("STRP"_mn).write(static_cast<std::uint32_t>(indices.size()),
                                       gsl::make_span(indices));
}

void write_segm(Ucfb_writer& geom, const scene::Geometry& geometry,
                const std::size_t material_index)
{
   auto segm = geom.emplace_child("SEGM"_mn);

   segm.emplace_child("MATI"_mn).write(static_cast<std::uint32_t>(material_index));

   const auto vertex_count = static_cast<std::uint32_t>(geometry.vertices.size);

   if (geometry.vertices.positions) {
      segm.emplace_child("POSL"_mn).write(
         vertex_count, gsl::make_span(geometry.vertices.positions.get(), vertex_count));
   }

   if (geometry.vertices.bones) write_wght(segm, geometry.vertices);

   if (geometry.vertices.normals) {
      segm.emplace_child("NRML"_mn).write(
         vertex_count, gsl::make_span(geometry.vertices.normals.get(), vertex_count));
   }

   if (geometry.vertices.static_lighting) {
      write_clrl(segm, gsl::make_span(geometry.vertices.colors.get(), vertex_count));
   }
   else if (geometry.vertices.colors) {
      write_clrl(segm, gsl::make_span(geometry.vertices.colors.get(), vertex_count));
   }

   if (geometry.vertices.texcoords) {
      segm.emplace_child("UV0L"_mn).write(
         vertex_count, gsl::make_span(geometry.vertices.texcoords.get(), vertex_count));
   }

   write_ndxl_ndxt(segm, geometry);
   write_strp(segm, geometry);
}

void write_envl(Ucfb_writer& geom, const std::vector<std::uint8_t>& bonemap)
{
   auto envl = geom.emplace_child("ENVL"_mn);

   envl.write(static_cast<std::uint32_t>(bonemap.size()));

   for (const std::uint32_t b : bonemap) envl.write(b);
}

void write_fwgt(Ucfb_writer& clth, const std::vector<std::string>& fixed_weights)
{
   auto fwgt = clth.emplace_child("FWGT"_mn);

   fwgt.write(static_cast<std::uint32_t>(fixed_weights.size()));

   for (const auto& str : fixed_weights) fwgt.write_unaligned(str);
}

void write_constraints(Ucfb_writer& clth, const Magic_number mn,
                       const std::vector<std::array<std::uint32_t, 2>>& constraints)
{
   auto writer = clth.emplace_child(mn);

   writer.write(static_cast<std::uint32_t>(constraints.size()));

   for (const auto c : constraints) {
      writer.write(static_cast<std::uint16_t>(c[0]), static_cast<std::uint16_t>(c[1]));
   }
}

void write_coll(Ucfb_writer& clth,
                const std::vector<Cloth_collision_primitive>& collision)
{
   auto coll = clth.emplace_child("COLL"_mn);

   coll.write(static_cast<std::uint32_t>(collision.size()));

   for (std::size_t i = 0; i < collision.size(); ++i) {
      coll.write(fmt::format("cloth_collision{}"sv, i));
      coll.write(collision[i].parent);
      coll.write(collision[i].type);
      coll.write(collision[i].size);
   }
}

void write_clth(Ucfb_writer& geom, const scene::Cloth_geometry& cloth_geometry)
{
   auto clth = geom.emplace_child("CLTH"_mn);

   clth.emplace_child("CTEX"_mn).write(
      fmt::format("{}.tga"sv, cloth_geometry.texture_name));

   const auto vertex_count = static_cast<std::uint32_t>(cloth_geometry.vertices.size);

   clth.emplace_child("CPOS"_mn).write(
      vertex_count,
      gsl::make_span(cloth_geometry.vertices.positions.get(), vertex_count));
   clth.emplace_child("CUV0"_mn).write(
      vertex_count,
      gsl::make_span(cloth_geometry.vertices.texcoords.get(), vertex_count));
   clth.emplace_child("FIDX"_mn).write(
      static_cast<std::uint32_t>(cloth_geometry.fixed_points.size()),
      gsl::make_span(cloth_geometry.fixed_points));
   write_fwgt(clth, cloth_geometry.fixed_weights);
   clth.emplace_child("CMSH"_mn).write(
      static_cast<std::uint32_t>(cloth_geometry.indices.size()),
      gsl::make_span(cloth_geometry.indices));

   write_constraints(clth, "SPRS"_mn, cloth_geometry.stretch_constraints);
   write_constraints(clth, "CPRS"_mn, cloth_geometry.cross_constraints);
   write_constraints(clth, "BPRS"_mn, cloth_geometry.bend_constraints);

   write_coll(clth, cloth_geometry.collision);
}

void write_geom(Ucfb_writer& modl, const scene::Node& node)
{
   auto geom = modl.emplace_child("GEOM"_mn);

   write_bbox(geom, node.aabb);

   if (node.geometry) {
      write_segm(geom, *node.geometry, node.material_index);

      if (!node.geometry->bone_map.empty()) write_envl(geom, node.geometry->bone_map);
   }

   if (node.cloth_geometry) write_clth(geom, *node.cloth_geometry);
}

void write_swci(Ucfb_writer& modl, const scene::Collision& collision)
{
   modl.emplace_child("SWCI"_mn).write(collision.type, collision.size);
}

void write_modl(Ucfb_writer& msh2, const scene::Node& node, const std::uint32_t index)
{
   auto modl = msh2.emplace_child("MODL"_mn);

   modl.emplace_child("MTYP"_mn).write(get_model_type(node));
   modl.emplace_child("MNDX"_mn).write<std::uint32_t>(index);
   modl.emplace_child("NAME"_mn).write(node.name);
   if (!node.parent.empty()) modl.emplace_child("PRNT"_mn).write(node.parent);
   if (is_hidden(node)) modl.emplace_child("FLGS"_mn).write(Model_flags::hidden);
   write_tran(modl, node.transform);
   if (node.geometry || node.cloth_geometry) write_geom(modl, node);
   if (node.collision) write_swci(modl, *node.collision);
}

void save_option_file(const scene::Scene& scene, File_saver& file_saver)
{
   auto output =
      file_saver.open_save_file("msh"sv, scene.name, ".msh.option"sv, std::ios::out);

   if (scene.vertex_lighting) output << "-vertexlighting"sv << '\n';
   if (scene.softskin) output << "-softskin"sv << '\n';

   for (const auto& light : scene.attached_lights) {
      fmt::format_to(std::ostream_iterator<char>{output}, "-attachlight \"{} {}\"\n"sv,
                     light.node, light.light);
   }

   if (!scene.nodes.empty()) {
      bool first = true;

      for (const auto& node : scene.nodes) {
         if (node.type != scene::Node_type::null || node.parent.empty() ||
             begins_with(node.name, "bone"sv) || begins_with(node.name, "hp"sv)) {
            continue;
         }

         if (std::exchange(first, false)) output << "-keep "sv;

         output << node.name << ' ';
      }

      if (!first) output << '\n';
   }

   if (!scene.nodes.empty()) {
      bool first = true;

      for (const auto& material : scene.materials) {
         if (!material.reference_in_option_file) continue;

         if (std::exchange(first, false)) output << "-keepmaterial "sv;

         output << material.name << ' ';
      }

      if (!first) output << '\n';
   }

   if (!scene::has_collision_geometry(scene)) {
      output << "-nocollision"sv << '\n';
   }
}

}

void save_scene(scene::Scene scene, File_saver& file_saver,
                [[maybe_unused]] const Game_version game_version)
{
   auto output = file_saver.open_save_file("msh"sv, scene.name, ".msh"sv);

   Ucfb_writer writer{output, "HEDR"_mn};

   // MSH2
   {
      auto msh2 = writer.emplace_child("MSH2"_mn);

      const auto previous_names_lut = make_model_name_lookup_table(scene.nodes);

      sort_nodes(scene.nodes);
      patch_bone_maps(scene.nodes, previous_names_lut);

      write_sinf(msh2, scene);
      write_matl(msh2, scene);

      for (std::uint32_t i = 0; i < scene.nodes.size(); ++i) {
         write_modl(msh2, scene.nodes[i], i);
      }
   }

   // CL1L
   (void)writer.emplace_child("CL1L"_mn);

   save_option_file(scene, file_saver);
}
}