
#include "msh_builder.hpp"
#include "bit_flags.hpp"
#include "byte.hpp"
#include "cloth_converter.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"
#include "ucfb_builder.hpp"

#include "tbb/parallel_for_each.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

using namespace std::literals;

namespace {

using namespace msh;

enum class Model_type : std::uint32_t {
   null = 0,
   skin = 1,
   fixed = 4,
   bone = 3,
   shadow = 6
};

struct Modl_collision {
   Primitive_type type;
   glm::vec3 size;
};

struct Modl_section {
   Model_type type;
   std::uint32_t index;
   std::uint32_t mat_index = 0;

   std::string name;
   std::string parent;

   glm::vec3 translation{0.0f, 0.0f, 0.0f};
   glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

   std::optional<Material> material;
   std::optional<std::vector<std::uint16_t>> strips;
   std::optional<std::vector<glm::vec3>> positions;
   std::optional<std::vector<glm::vec3>> normals;
   std::optional<std::vector<std::array<std::uint8_t, 4>>> colours;
   std::optional<std::vector<glm::vec2>> texture_coords;
   std::optional<std::vector<Skin_entry>> skin;
   std::optional<std::vector<std::uint32_t>> bone_map;
   std::optional<Cloth> cloth;
   std::optional<Modl_collision> collision;
};

auto create_bone_index(
   const std::unordered_map<std::string, std::uint32_t>& old_index_map,
   const std::vector<Modl_section>& sections)
   -> std::unordered_map<std::uint32_t, std::uint32_t>
{
   std::unordered_map<std::uint32_t, std::uint32_t> bone_indices;

   for (const auto& section : sections) {
      const auto old_index = old_index_map.find(section.name);

      if (old_index == std::cend(old_index_map)) {
         throw std::runtime_error{"Ill-formed mesh hierarchy."};
      }

      bone_indices[old_index->second] = section.index;
   }

   return bone_indices;
}

template<typename Type>
std::vector<Type> downgrade_concurrent_vector(const tbb::concurrent_vector<Type>& from)
{
   std::vector<Type> to;
   to.reserve(from.size());

   for (const auto& item : from) {
      to.push_back(item);
   }

   return to;
}

const Bone& find_root_bone(const std::vector<Bone>& bones)
{
   const auto root = std::find_if(std::cbegin(bones), std::cend(bones),
                                  [](const Bone& bone) { return bone.parent.empty(); });

   if (root == std::cend(bones)) {
      throw std::runtime_error{"Unable to find root bone."};
   }

   return *root;
}

auto sort_sections(std::vector<Modl_section>& sections)
   -> std::unordered_map<std::uint32_t, std::uint32_t>
{
   std::unordered_map<std::string, std::uint32_t> old_index_map;

   for (const auto& section : sections) {
      old_index_map[section.name] = section.index;
   }

   std::unordered_map<std::string, std::vector<Modl_section>> children_map;

   for (const auto& section : sections) {
      children_map[section.parent].emplace_back(std::move(section));
   }

   auto& root_bone = children_map.at(""s);

   std::function<void(std::vector<Modl_section>&)> read_bone =
      [&read_bone, &children_map, &sections](std::vector<Modl_section>& child_bones) {
         while (!child_bones.empty()) {
            sections.emplace_back(std::move(child_bones.back()));
            child_bones.pop_back();

            sections.back().index = static_cast<std::uint32_t>(sections.size());

            auto children = children_map.find(sections.back().name);

            if (children != std::end(children_map)) {
               read_bone(children->second);
            }
         }
      };

   sections.clear();
   read_bone(root_bone);

   return create_bone_index(old_index_map, sections);
}

template<typename Type>
void reverse_pretransformed(std::vector<Type>& meshes, const std::vector<Bone>& bones)
{
   for (auto& mesh : meshes) {
      if (!mesh.pretransformed || mesh.skin.empty()) continue;

      if (mesh.skin.size() != mesh.positions.size() ||
          mesh.skin.size() != mesh.normals.size()) {
         throw std::runtime_error{
            "Count of segment's skin entries and vertex entries does not match"};
      }

      for (std::size_t i = 0; i < mesh.skin.size(); ++i) {
         auto& position = mesh.positions[i];
         auto& normal = mesh.normals[i];

         const auto bone_index = mesh.bone_map.at(mesh.skin[i].bones[0]);
         auto bone = bones.begin() + bone_index;

         while (bone < std::end(bones)) {
            position = position * glm::inverse(bone->rotation);
            normal = normal * glm::inverse(bone->rotation);

            position += bone->position;

            bone = std::find_if(
               std::begin(bones), std::end(bones),
               [bone](const Bone& other) { return (other.name == bone->parent); });
         }

         normal = glm::normalize(normal);
      }
   }
}

std::size_t count_strips_indices(const std::vector<std::vector<std::uint16_t>>& strips)
{
   std::size_t count = 0;

   for (const auto& strip : strips) {
      count += strip.size();
   }

   return count;
}

std::vector<std::uint16_t> strips_to_msh_fmt(
   const std::vector<std::vector<std::uint16_t>>& strips)
{
   const auto size = count_strips_indices(strips);

   std::vector<std::uint16_t> msh_strips;
   msh_strips.reserve(size);

   for (const auto& strip : strips) {
      if (strip.size() < 3) throw std::runtime_error{"strip in model was too short"};

      msh_strips.push_back(strip[0] | 0x8000);
      msh_strips.push_back(strip[1] | 0x8000);

      msh_strips.insert(std::end(msh_strips), std::begin(strip) + 2, std::end(strip));
   }

   return msh_strips;
}

std::vector<std::uint32_t> convert_bonemap(const std::vector<std::uint8_t>& bmap)
{
   std::vector<std::uint32_t> result;
   result.reserve(bmap.size());

   for (const auto b : bmap) {
      result.push_back(b + 1);
   }

   return result;
}

void remap_bonemap(std::vector<std::uint32_t>& bmap,
                   const std::unordered_map<std::uint32_t, std::uint32_t>& bone_index)
{
   for (auto& b : bmap) {
      b = bone_index.at(b);
   }
}

void fixup_texture_names(Material& material)
{
   auto& textures = material.textures;

   for (auto& texture : textures) {
      if (!texture.empty()) texture += ".tga"_sv;
   }
}

std::string create_coll_flags_name(Collision_flags flags)
{
   if (flags == Collision_flags::all) return ""s;

   std::string name{"-"};

   if (are_flags_set(flags, Collision_flags::soldier)) name += 's';
   if (are_flags_set(flags, Collision_flags::vehicle)) name += 'v';
   if (are_flags_set(flags, Collision_flags::building)) name += 'b';
   if (are_flags_set(flags, Collision_flags::terrain)) name += 't';
   if (are_flags_set(flags, Collision_flags::ordnance)) name += 'o';
   if (are_flags_set(flags, Collision_flags::flyer)) name += 'f';

   name += "-_"_sv;

   return name;
}

Modl_section create_section_from(const Bone& bone, std::string_view, std::uint32_t index)
{
   Modl_section section;

   section.type = Model_type::bone;
   section.index = index;
   section.name = bone.name;
   section.parent = bone.parent;
   section.translation = bone.position;
   section.rotation = bone.rotation;

   return section;
}

Modl_section create_section_from(const Model& model, std::string_view root_name,
                                 std::uint32_t index)
{
   Modl_section section;

   section.type = Model_type::fixed;
   section.index = index;

   if (!model.name) {
      section.name = "mesh_"s;

      if (model.low_resolution) section.name += "lowrez_"_sv;
      section.name += std::to_string(index);
   }
   else {
      section.name = model.name.value();
   }

   section.parent = model.parent.value_or(std::string{root_name});

   section.rotation = model.rotation;
   section.translation = model.position;

   section.material = model.material;
   fixup_texture_names(*section.material);

   section.strips = strips_to_msh_fmt(model.strips);
   section.positions = model.positions;
   section.normals = model.normals;

   if (!model.colours.empty()) {
      section.colours = model.colours;
   }
   if (!model.texture_coords.empty()) {
      section.texture_coords = model.texture_coords;
   }
   if (!model.skin.empty()) {
      section.skin = model.skin;
      section.type = Model_type::skin;
   }
   if (!model.bone_map.empty()) {
      section.bone_map = convert_bonemap(model.bone_map);
   }

   return section;
}

Modl_section create_section_from(const Collsion_mesh& collision,
                                 std::string_view root_name, std::uint32_t index)
{
   Modl_section section;

   section.type = Model_type::fixed;
   section.index = index;
   section.name =
      "collision_"s + create_coll_flags_name(collision.flags) + std::to_string(index);
   section.parent = collision.parent.value_or(std::string{root_name});

   section.strips = strips_to_msh_fmt(collision.strips);
   section.positions = collision.positions;

   return section;
}

Modl_section create_section_from(const Collision_primitive& primitive, std::string_view,
                                 std::uint32_t index)
{
   Modl_section section;

   section.type = Model_type::null;
   section.index = index;
   section.name = "p_"s + create_coll_flags_name(primitive.flags) + std::to_string(index);
   section.parent = primitive.parent;
   section.translation = primitive.position;
   section.rotation = primitive.rotation;
   section.collision = {primitive.type, primitive.size};

   return section;
}

Modl_section create_section_from(const Cloth& cloth, std::string_view,
                                 std::uint32_t index)
{
   Modl_section section;

   section.type = Model_type::fixed;
   section.index = index;
   section.name = cloth.name;
   section.parent = cloth.parent;
   section.translation = cloth.position;
   section.rotation = cloth.rotation;
   section.cloth = cloth;
   section.cloth->texture_name += ".tga"s;

   return section;
}

std::vector<Modl_section> create_modl_sections(
   std::vector<Bone> bones, std::vector<Model> models,
   std::vector<Collsion_mesh> collision_meshes,
   std::vector<Collision_primitive> collision_primitives, std::vector<Cloth> cloths)
{
   reverse_pretransformed(models, bones);

   std::vector<Modl_section> sections;
   sections.reserve(bones.size() + models.size());

   const auto root_name = find_root_bone(bones).name;
   std::uint32_t model_index = 1;

   const auto create_section = [&sections, &model_index, &root_name](auto& item) {
      sections.emplace_back(create_section_from(item, root_name, model_index));

      model_index += 1;
   };

   std::for_each(std::begin(bones), std::end(bones), create_section);
   std::for_each(std::begin(models), std::end(models), create_section);
   std::for_each(std::begin(collision_meshes), std::end(collision_meshes),
                 create_section);
   std::for_each(std::begin(collision_primitives), std::end(collision_primitives),
                 create_section);
   std::for_each(std::begin(cloths), std::end(cloths), create_section);

   const auto bone_index = sort_sections(sections);

   for (auto& section : sections) {
      if (section.bone_map) {
         remap_bonemap(*section.bone_map, bone_index);
      }
   }

   return sections;
}

template<Magic_number magic_number>
Ucfb_builder create_pos_chunk(const std::vector<glm::vec3>& positions)
{
   Ucfb_builder pos{magic_number};
   pos.write(static_cast<std::uint32_t>(positions.size()));

   for (const auto& v : positions) {
      pos.write_multiple(v.x, v.y, v.z);
   }

   return pos;
}

template<Magic_number magic_number>
Ucfb_builder create_uv_chunk(const std::vector<glm::vec2>& texture_coords)
{
   Ucfb_builder uvl{magic_number};
   uvl.write(static_cast<std::uint32_t>(texture_coords.size()));

   for (const auto& uv : texture_coords) {
      uvl.write_multiple(uv.x, uv.y);
   }

   return uvl;
}

Ucfb_builder create_wght_chunk(const std::vector<Skin_entry>& skin)
{
   Ucfb_builder wght{"WGHT"_mn};
   wght.write(static_cast<std::uint32_t>(skin.size()));

   for (const auto& entry : skin) {
      const auto bones = static_cast<glm::uvec3>(entry.bones);
      const auto& weights = entry.weights;

      wght.write_multiple(bones.x, weights.x, bones.y, weights.y, bones.z, weights.z,
                          0ui32, 0.0f);
   }

   return wght;
}

Ucfb_builder create_nrml_chunk(const std::vector<glm::vec3>& normals)
{
   Ucfb_builder nrml{"NRML"_mn};
   nrml.write(static_cast<std::uint32_t>(normals.size()));

   for (const auto& n : normals) {
      nrml.write_multiple(n.x, n.y, n.z);
   }

   return nrml;
}

Ucfb_builder create_clrl_chunk(const std::vector<std::array<std::uint8_t, 4>>& colours)
{
   Ucfb_builder clrl{"CLRL"_mn};
   clrl.write(static_cast<std::uint32_t>(colours.size()));

   for (const auto& c : colours) {
      clrl.write_multiple(c[0], c[1], c[2], c[3]);
   }

   return clrl;
}

Ucfb_builder create_strp_chunk(const std::vector<std::uint16_t>& strips)
{
   Ucfb_builder strp{"STRP"_mn};
   strp.write(static_cast<std::uint32_t>(strips.size()));

   for (const auto indx : strips) {
      strp.write(indx);
   }

   strp.pad_till_aligned();

   return strp;
}

Ucfb_builder create_segm_chunk(const Modl_section& section)
{
   Ucfb_builder segm{"SEGM"_mn};

   segm.emplace_child("MATI"_mn).write(section.mat_index);

   if (section.positions) segm.add_child(create_pos_chunk<"POSL"_mn>(*section.positions));
   if (section.skin) segm.add_child(create_wght_chunk(*section.skin));
   if (section.normals) segm.add_child(create_nrml_chunk(*section.normals));
   if (section.colours) segm.add_child(create_clrl_chunk(*section.colours));
   if (section.texture_coords)
      segm.add_child(create_uv_chunk<"UV0L"_mn>(*section.texture_coords));
   if (section.strips) segm.add_child(create_strp_chunk(*section.strips));

   return segm;
}

Ucfb_builder create_fidx_chunk(const std::vector<std::uint32_t>& fixed_points)
{
   Ucfb_builder fidx{"FIDX"_mn};
   fidx.write(static_cast<std::uint32_t>(fixed_points.size()));

   for (const auto& point : fixed_points) {
      fidx.write(point);
   }

   return fidx;
}

Ucfb_builder create_fwgt_chunk(const std::vector<std::string>& fixed_weights)
{
   Ucfb_builder fwgt{"FWGT"_mn};
   fwgt.write(static_cast<std::uint32_t>(fixed_weights.size()));

   for (const auto& name : fixed_weights) {
      fwgt.write(name, true, false);
   }

   fwgt.pad_till_aligned();

   return fwgt;
}

Ucfb_builder create_cmsh_chunk(const std::vector<glm::uvec3>& indices)
{
   Ucfb_builder cmsh{"CMSH"_mn};
   cmsh.write(static_cast<std::uint32_t>(indices.size()));

   for (const auto& i : indices) {
      cmsh.write_multiple(i.x, i.y, i.z);
   }

   return cmsh;
}

template<Magic_number magic_number>
Ucfb_builder create_constraint_chunk(
   const std::vector<std::array<std::uint16_t, 2>>& constraints)
{
   Ucfb_builder con{magic_number};
   con.write(static_cast<std::uint32_t>(constraints.size()));

   for (const auto& c : constraints) {
      con.write_multiple(c[0], c[1]);
   }

   return con;
}

Ucfb_builder create_coll_chunk(const std::vector<Cloth_collision>& collision)
{
   Ucfb_builder coll{"COLL"_mn};
   coll.write(static_cast<std::uint32_t>(collision.size()));

   std::size_t i = 0;

   for (const auto& node : collision) {
      coll.write(node.parent + "cloth_"s + std::to_string(i), false, true);
      coll.write(node.parent, true, false);
      coll.write(node.type);
      coll.write_multiple(node.size.x, node.size.y, node.size.z);

      ++i;
   }

   return coll;
}

Ucfb_builder create_clth_chunk(const Cloth& cloth_info)
{
   Ucfb_builder clth{"CLTH"_mn};

   clth.emplace_child("CTEX"_mn).write(cloth_info.texture_name);

   clth.add_child(create_pos_chunk<"CPOS"_mn>(cloth_info.positions));
   clth.add_child(create_uv_chunk<"CUV0"_mn>(cloth_info.texture_coords));
   clth.add_child(create_fidx_chunk(cloth_info.fixed_points));
   clth.add_child(create_fwgt_chunk(cloth_info.fixed_weights));
   clth.add_child(create_cmsh_chunk(cloth_info.indices));
   clth.add_child(create_constraint_chunk<"SPRS"_mn>(cloth_info.stretch_constraints));
   clth.add_child(create_constraint_chunk<"CPRS"_mn>(cloth_info.cross_constraints));
   clth.add_child(create_constraint_chunk<"BPRS"_mn>(cloth_info.bend_constraints));
   clth.add_child(create_coll_chunk(cloth_info.collision));

   return clth;
}

Ucfb_builder create_envl_chunk(const std::vector<std::uint32_t>& bone_map)
{
   Ucfb_builder envl{"ENVL"_mn};
   envl.write(static_cast<std::uint32_t>(bone_map.size()));

   for (const auto bone : bone_map) {
      envl.write(bone);
   }

   return envl;
}

Ucfb_builder create_geom_chunk(const Modl_section& section)
{
   Ucfb_builder geom{"GEOM"_mn};

   auto& bbox = geom.emplace_child("BBOX"_mn);
   bbox.write_multiple(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

   if (!section.cloth)
      geom.add_child(create_segm_chunk(section));
   else
      geom.add_child(create_clth_chunk(*section.cloth));

   if (section.bone_map) geom.add_child(create_envl_chunk(*section.bone_map));

   return geom;
}

Ucfb_builder create_swci_chunk(const Modl_collision& collsion)
{
   Ucfb_builder swci{"SWCI"_mn};

   swci.write(collsion.type);
   swci.write_multiple(collsion.size.x, collsion.size.y, collsion.size.z);

   return swci;
}

Ucfb_builder create_modl_chunk(const Modl_section& section)
{
   Ucfb_builder modl{"MODL"_mn};

   modl.emplace_child("MTYP"_mn).write(section.type);
   modl.emplace_child("MNDX"_mn).write(section.index);
   modl.emplace_child("NAME"_mn).write(section.name);

   if (!section.parent.empty()) {
      modl.emplace_child("PRNT"_mn).write(section.parent);
   }

   auto& tran = modl.emplace_child("TRAN"_mn);
   tran.write_multiple(1.0f, 1.0f, 1.0f);
   tran.write_multiple(section.rotation.x, section.rotation.y, section.rotation.z,
                       section.rotation.w);
   tran.write_multiple(section.translation.x, section.translation.y,
                       section.translation.z);

   if (section.type != Model_type::null && section.type != Model_type::bone) {
      modl.add_child(create_geom_chunk(section));
   }

   if (section.collision) {
      modl.add_child(create_swci_chunk(*section.collision));
   }

   return modl;
}

Ucfb_builder create_matd_chunk(const Material& material, std::size_t index)
{
   Ucfb_builder matd{"MATD"_mn};

   auto& name = matd.emplace_child("NAME"_mn);
   name.write("material_"s + std::to_string(index));

   auto& data = matd.emplace_child("DATA"_mn);
   data.write_multiple(material.diffuse_colour.r, material.diffuse_colour.g,
                       material.diffuse_colour.b, material.diffuse_colour.a);
   data.write_multiple(material.specular_colour.r, material.specular_colour.g,
                       material.specular_colour.b, material.specular_colour.a);
   data.write_multiple(1.0f, 1.0f, 1.0f, 1.0f);
   data.write(material.specular_value);

   auto& atrb = matd.emplace_child("ATRB"_mn);
   atrb.write(material.flags);
   atrb.write(material.type);
   atrb.write(material.params);

   for (auto i = 0; i < material.textures.size(); ++i) {
      static_assert(std::tuple_size_v<decltype(Material::textures)> <= 10,
                    "Max texture count can not be above 10!");

      auto& tex = matd.emplace_child(
         create_magic_number('T', 'X', '0' + static_cast<char>(i), 'D'));

      if (!material.textures[i].empty()) {
         tex.write(material.textures[i]);
      }
   }

   return matd;
}

Ucfb_builder create_matl_chunk(std::vector<Modl_section>& sections)
{
   std::vector<const Material*> materials;

   for (auto& section : sections) {
      if (section.material) {
         section.mat_index = static_cast<std::uint32_t>(materials.size());
         materials.push_back(&section.material.value());
      }
   }

   Ucfb_builder matl{"MATL"_mn};

   matl.write(static_cast<std::uint32_t>(materials.size()));

   for (std::size_t i = 0; i < materials.size(); ++i) {
      matl.add_child(create_matd_chunk(*materials[i], i));
   }

   return matl;
}

Ucfb_builder create_sinf_chunk(const Bbox msh_bbox, std::string_view model_name)
{
   Ucfb_builder sinf{"SINF"_mn};

   auto& name = sinf.emplace_child("NAME"_mn);
   name.write(model_name);

   auto& fram = sinf.emplace_child("FRAM"_mn);

   fram.write_multiple(0i32, 1i32, 29.97003f);

   auto& bbox = sinf.emplace_child("BBOX"_mn);

   const auto& rotation = msh_bbox.rotation;

   bbox.write_multiple(rotation.x, rotation.y, rotation.z, rotation.w);
   bbox.write_multiple(msh_bbox.centre.x, msh_bbox.centre.y, msh_bbox.centre.z);
   bbox.write_multiple(msh_bbox.size.x, msh_bbox.size.y, msh_bbox.size.z);
   bbox.write(std::max({msh_bbox.size.x, msh_bbox.size.y, msh_bbox.size.z}) * 2.0f);

   return sinf;
}

std::string create_msh_file(std::vector<Modl_section> sections, const Bbox bbox,
                            std::string_view name)
{
   Ucfb_builder hedr{"HEDR"_mn};

   auto& msh2 = hedr.emplace_child("MSH2"_mn);

   msh2.add_child(create_sinf_chunk(bbox, name));
   msh2.add_child(create_matl_chunk(sections));

   for (const auto& section : sections) {
      msh2.add_child(create_modl_chunk(section));
   }

   hedr.emplace_child("CL1L"_mn);

   return hedr.create_buffer();
}
}

namespace msh {

Builder::Builder(const Builder& other) : Builder{}
{
   this->_bones = other._bones;
   this->_models = other._models;
   this->_collision_meshes = other._collision_meshes;
   this->_collision_primitives = other._collision_primitives;
   this->_cloths = other._cloths;

   std::lock_guard<tbb::spin_mutex> bbox_lock{other._bbox_mutex};
   this->_bbox = other._bbox;
}

void Builder::add_bone(Bone bone)
{
   _bones.emplace_back(std::move(bone));
}

void Builder::add_model(Model model)
{
   _models.emplace_back(std::move(model));
}

void Builder::add_collision_mesh(Collsion_mesh collision_mesh)
{
   _collision_meshes.emplace_back(std::move(collision_mesh));
}

void Builder::add_collision_primitive(Collision_primitive primitive)
{
   _collision_primitives.emplace_back(std::move(primitive));
}

void Builder::add_cloth(Cloth cloth)
{
   _cloths.emplace_back(std::move(cloth));
}

void Builder::set_bbox(const Bbox& bbox) noexcept
{
   std::lock_guard<tbb::spin_mutex> bbox_lock{_bbox_mutex};
   _bbox = bbox;
}

void Builder::save(const std::string& name, File_saver& file_saver,
                   const Game_version version) const
{
   auto bones = downgrade_concurrent_vector(_bones);
   auto models = downgrade_concurrent_vector(_models);
   auto cloths = downgrade_concurrent_vector(_cloths);
   auto collision_meshes = downgrade_concurrent_vector(_collision_meshes);
   auto collision_primitives = downgrade_concurrent_vector(_collision_primitives);

   if (version == Game_version::swbf) {
      for (const auto& cloth : cloths) {
         models.emplace_back(cloth_to_model(cloth, bones));
      }

      cloths.clear();
   }

   auto msh_file =
      create_msh_file(create_modl_sections(
                         std::move(bones), std::move(models), std::move(collision_meshes),
                         std::move(collision_primitives), std::move(cloths)),
                      get_bbox(), name);

   file_saver.save_file(msh_file, "msh"_sv, name, ".msh"_sv);
}

Bbox Builder::get_bbox() const noexcept
{
   std::lock_guard<tbb::spin_mutex> bbox_lock{_bbox_mutex};
   return _bbox;
}

void save_all(File_saver& file_saver, const Builders_map& builders,
              const Game_version version)
{
   const auto functor = [&file_saver,
                         version](const std::pair<std::string, Builder>& builder) {
      try {
         builder.second.save(builder.first, file_saver, version);
      }
      catch (const std::exception& e) {
         synced_cout::print("Error: Exception occured while saving ", builder.first,
                            ".msh\n   Message: "s, e.what(), '\n');
      }
   };

   tbb::parallel_for_each(builders, functor);
}
}
