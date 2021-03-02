
#include "model_builder.hpp"
#include "app_options.hpp"
#include "model_basic_primitives.hpp"
#include "model_gltf_save.hpp"
#include "model_msh_save.hpp"
#include "model_scene.hpp"
#include "synced_cout.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <tuple>

#include <fmt/format.h>
#include <gsl/gsl>
#include <tbb/parallel_for_each.h>

using namespace std::literals;

namespace model {

namespace {

template<typename Type>
void merge_containers(std::vector<Type>& into, std::vector<Type>& from) noexcept
{
   into.insert(into.end(), std::make_move_iterator(from.begin()),
               std::make_move_iterator(from.end()));

   from.clear();
}

auto lod_suffix(const Lod lod) -> std::string_view
{
   switch (lod) {
   case Lod::zero:
      return ""sv;
   case Lod::one:
      return "_lod1"sv;
   case Lod::two:
      return "_lod2"sv;
   case Lod::three:
      return "_lod3"sv;
   case Lod::lowres:
      return "_lowres"sv;
   default:
      std::terminate();
   }
}

auto collision_flags_string(const Collision_flags flags) -> std::string
{
   if (flags == Collision_flags::all) return ""s;

   std::string str;

   if (are_flags_set(flags, Collision_flags::soldier)) str += "s"sv;
   if (are_flags_set(flags, Collision_flags::vehicle)) str += "v"sv;
   if (are_flags_set(flags, Collision_flags::building)) str += "b"sv;
   if (are_flags_set(flags, Collision_flags::terrain)) str += "t"sv;
   if (are_flags_set(flags, Collision_flags::ordnance)) str += "o"sv;
   if (are_flags_set(flags, Collision_flags::flyer)) str += "f"sv;

   return str;
}

auto insert_scene_material(scene::Scene& scene, scene::Material material) -> std::size_t
{
   if (auto it = std::find(scene.materials.begin(), scene.materials.end(), material);
       it != scene.materials.end()) {
      return static_cast<std::size_t>(std::distance(scene.materials.begin(), it));
   }

   const auto index = scene.materials.size();

   scene.materials.emplace_back(std::move(material));

   return index;
}

auto make_primitive_visualization_geometry(const Collision_primitive_type type,
                                           const glm::vec3 size) -> scene::Geometry
{
   const auto [indices, positions, normals, texcoords, scale] = [=] {
      using namespace primitives;

      const auto as_spans = [](const auto&... args) {
         return std::make_tuple(gsl::make_span(args)...);
      };

      switch (type) {
      case Collision_primitive_type::cylinder:
         return std::tuple_cat(as_spans(cylinder_indices, cylinder_vertex_positions,
                                        cylinder_vertex_normals,
                                        cylinder_vertex_texcoords),
                               std::make_tuple(glm::vec3{size.xyx}));
      case Collision_primitive_type::cube:
         return std::tuple_cat(as_spans(cube_indices, cube_vertex_positions,
                                        cube_vertex_normals, cube_vertex_texcoords),
                               std::make_tuple(glm::vec3{size}));
      default:
         return std::tuple_cat(as_spans(sphere_indices, sphere_vertex_positions,
                                        sphere_vertex_normals, sphere_vertex_texcoords),
                               std::make_tuple(glm::vec3{size.x}));
      }
   }();

   scene::Geometry geometry{
      .topology = primitives::primitive_topology,
      .indices = Indices{std::cbegin(indices), std::cend(indices)},
      .vertices = Vertices{static_cast<std::size_t>(positions.size()),
                           {.positions = true, .normals = true, .texcoords = true}}};

   std::transform(std::cbegin(positions), std::cend(positions),
                  geometry.vertices.positions.get(),
                  [scale](const auto pos) { return pos * scale; });
   std::copy(std::cbegin(normals), std::cend(normals), geometry.vertices.normals.get());
   std::copy(std::cbegin(texcoords), std::cend(texcoords),
             geometry.vertices.texcoords.get());

   return geometry;
}

auto positions_to_vertices(const std::vector<glm::vec3>& positions) -> Vertices
{
   Vertices vertices{positions.size(), {.positions = true}};

   std::copy(std::cbegin(positions), std::cend(positions), vertices.positions.get());

   return vertices;
}

auto create_scene(Model model) -> scene::Scene
{
   scene::Scene scene{.name = std::move(model.name)};

   scene.materials.push_back({.name = "default_material",
                              .diffuse_colour = {0.5f, 0.5f, 0.5f, 0.33f},
                              .flags = Render_flags::transparent});

   if (model.bones.empty()) {
      scene.nodes.push_back({.name = "root"s,
                             .parent = ""s,
                             .material_index = 0,
                             .type = scene::Node_type::null});
   }

   for (auto& bone : model.bones) {
      scene.nodes.push_back({.name = std::move(bone.name),
                             .parent = std::move(bone.parent),
                             .material_index = 0,
                             .type = scene::Node_type::null,
                             .transform = bone.transform});
   }

   int name_counter = 0;

   for (auto& part : model.parts) {
      if (part.material.attached_light) {
         scene.attached_lights.push_back(
            {.node = part.name.value(),
             .light = std::move(part.material.attached_light.value())});
      }

      scene.nodes.push_back(
         {.name = part.name ? std::move(*part.name)
                            : fmt::format("mesh_part{}{}"sv, ++name_counter,
                                          lod_suffix(part.lod)),
          .parent = std::move(part.parent),
          .material_index = insert_scene_material(
             scene,
             scene::Material{.name = part.material.name.value_or(""s),
                             .diffuse_colour = part.material.diffuse_colour,
                             .specular_colour = part.material.specular_colour,
                             .specular_exponent = part.material.specular_exponent,
                             .flags = part.material.flags,
                             .rendertype = part.material.type,
                             .params = part.material.params,
                             .textures = std::move(part.material.textures),
                             .reference_in_option_file = part.material.name.has_value()}),
          .type = scene::Node_type::geometry,
          .lod = part.lod,
          .geometry = scene::Geometry{.topology = part.primitive_topology,
                                      .indices = std::move(part.indices),
                                      .vertices = std::move(part.vertices),
                                      .bone_map = std::move(part.bone_map)}});
   }

   name_counter = 0;

   for (auto& mesh : model.collision_meshes) {
      scene.nodes.push_back(
         {.name = fmt::format("collision_-{}-mesh{}"sv,
                              collision_flags_string(mesh.flags), ++name_counter),
          .parent = scene.nodes.front().name, // take the dangerous assumption that the
                                              // first node we added is root
          .material_index = 0,
          .type = scene::Node_type::collision,
          .geometry =
             scene::Geometry{.topology = mesh.primitive_topology,
                             .indices = std::move(mesh.indices),
                             .vertices = positions_to_vertices(mesh.positions)}});
   }

   for (auto& prim : model.collision_primitives) {
      scene.nodes.push_back(
         {.name = prim.name,
          .parent = std::move(prim.parent),
          .material_index = 0,
          .type = scene::Node_type::collision_primitive,
          .transform = prim.transform,
          .geometry = make_primitive_visualization_geometry(prim.type, prim.size),
          .collision = scene::Collision{.type = prim.type, .size = prim.size}});
   }

   for (auto& cloth : model.cloths) {
      scene.nodes.push_back(
         {.name = std::move(cloth.name),
          .parent = std::move(cloth.parent),
          .material_index = 0,
          .type = scene::Node_type::cloth_geometry,
          .transform = cloth.transform,
          .cloth_geometry = scene::Cloth_geometry{
             .texture_name = std::move(cloth.texture_name),
             .vertices = std::move(cloth.vertices),
             .indices = std::move(cloth.indices),
             .fixed_points = std::move(cloth.fixed_points),
             .fixed_weights = std::move(cloth.fixed_weights),
             .stretch_constraints = std::move(cloth.stretch_constraints),
             .cross_constraints = std::move(cloth.cross_constraints),
             .bend_constraints = std::move(cloth.bend_constraints),
             .collision = std::move(cloth.collision),
          }});
   }

   name_counter = 0;

   for (auto& material : scene.materials) {
      if (!material.name.empty()) continue;

      material.name = fmt::format("material{}"sv, ++name_counter);
   }

   for (const auto& node : scene.nodes) {
      scene.softskin |= node.geometry ? node.geometry->vertices.softskinned : false;
      scene.vertex_lighting |=
         node.geometry ? node.geometry->vertices.static_lighting : false;
   }

   scene::reverse_pretransforms(scene);
   scene::recreate_aabbs(scene);

   return scene;
}

void save_model(Model model, File_saver& file_saver, const Game_version game_version,
                const Model_format format)
{
   if (format == Model_format::msh) {
      msh::save_scene(create_scene(std::move(model)), file_saver, game_version);
   }
   else if (format == Model_format::gltf2) {
      gltf::save_scene(create_scene(std::move(model)), file_saver);
   }
}

void clean_model(Model& model, const Model_discard_flags discard_flags) noexcept
{
   if (discard_flags == Model_discard_flags::none) return;

   if (are_flags_set(discard_flags, Model_discard_flags::collision)) {
      model.collision_meshes.clear();
      model.collision_primitives.clear();
   }

   if (are_flags_set(discard_flags, Model_discard_flags::lod)) {
      model.parts.erase(
         std::remove_if(model.parts.begin(), model.parts.end(),
                        [](const Part& part) { return part.lod != Lod::zero; }),
         model.parts.end());
   }
}
}

Vertices::Vertices(const std::size_t size, const Create_flags flags) : size{size}
{
   if (flags.positions) positions = std::make_unique<glm::vec3[]>(size);
   if (flags.normals) normals = std::make_unique<glm::vec3[]>(size);
   if (flags.tangents) tangents = std::make_unique<glm::vec3[]>(size);
   if (flags.bitangents) bitangents = std::make_unique<glm::vec3[]>(size);
   if (flags.colors) colors = std::make_unique<glm::vec4[]>(size);
   if (flags.texcoords) texcoords = std::make_unique<glm::vec2[]>(size);
   if (flags.bones) bones = std::make_unique<glm::u8vec3[]>(size);
   if (flags.weights) weights = std::make_unique<glm::vec3[]>(size);
}

void Model::merge_with(Model other) noexcept
{
   merge_containers(this->bones, other.bones);
   merge_containers(this->parts, other.parts);
   merge_containers(this->collision_meshes, other.collision_meshes);
   merge_containers(this->collision_primitives, other.collision_primitives);
   merge_containers(this->cloths, other.cloths);
}

void Models_builder::integrate(Model model) noexcept
{
   std::lock_guard lock{_mutex};

   if (const auto it = std::find_if(
          _models.begin(), _models.end(),
          [&](const auto& other) noexcept { return (model.name == other.name); });
       it != _models.end()) {
      it->merge_with(std::move(model));
   }
   else {
      _models.emplace_back(std::move(model));
   }
}

void Models_builder::save_models(File_saver& file_saver, const Game_version game_version,
                                 const Model_format format,
                                 const Model_discard_flags discard_flags) noexcept
{
   if (get_pre_processing_global()) return; // ---------early return----------------
   std::lock_guard lock{_mutex};

   tbb::parallel_for_each(_models, [&](Model& model) {
      const std::string name = model.name;

      try {
         clean_model(model, discard_flags);
         save_model(std::move(model), file_saver, game_version, format);
      }
      catch (std::exception& e) {
         synced_cout::print(
            fmt::format("Failed to save model {}! Reason: {}\n"sv, name, e.what()));
      }
   });

   _models.clear();
}

}
