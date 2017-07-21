
#include "msh_builder.hpp"
#include "bit_flags.hpp"
#include "byte.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
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
   std::optional<std::vector<glm::vec3>> vertices;
   std::optional<std::vector<glm::vec3>> normals;
   std::optional<std::vector<std::array<std::uint8_t, 4>>> colours;
   std::optional<std::vector<glm::vec2>> texture_coords;
   std::optional<std::vector<std::uint8_t>> skin;
   std::optional<std::vector<std::uint32_t>> bone_map;
   std::optional<Modl_collision> collision;
};

auto create_bone_index(
   const std::unordered_map<std::string, std::uint32_t>& old_index_map,
   const std::vector<Bone>& bones) -> std::vector<std::uint32_t>
{
   std::vector<std::uint32_t> bone_indices;
   bone_indices.resize(bones.size());

   for (std::uint32_t i = 0; i < bones.size(); ++i) {
      const auto old_index = old_index_map.find(bones[i].name);

      if (old_index == std::cend(old_index_map)) {
         throw std::runtime_error{"Ill-formed skeleton."};
      }

      bone_indices[old_index->second] = i + 1;
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

auto sort_bones(std::vector<Bone>& bones) -> std::vector<std::uint32_t>
{
   std::unordered_map<std::string, std::uint32_t> old_index_map;

   for (std::uint32_t i = 0; i < bones.size(); ++i) {
      old_index_map[bones[i].name] = i;
   }

   std::unordered_map<std::string, std::vector<Bone>> children_map;

   for (const auto& bone : bones) {
      children_map[bone.parent].emplace_back(std::move(bone));
   }

   auto root_bone = children_map.find(""s);

   if (root_bone == std::end(children_map)) {
      throw std::runtime_error{"Can't find root bone for skeleton."};
   }

   std::function<void(std::vector<Bone>&)> read_bone =
      [&read_bone, &children_map, &bones](std::vector<Bone>& child_bones) {
         for (auto& bone : child_bones) {
            bones.emplace_back(std::move(bone));

            auto children = children_map.find(bones.back().name);

            if (children != std::end(children_map)) {
               read_bone(children->second);
            }
         }
      };

   bones.clear();
   read_bone(root_bone->second);

   return create_bone_index(old_index_map, bones);
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

std::vector<std::uint32_t> remap_bonemap(const std::vector<std::uint8_t>& bmap,
                                         const std::vector<std::uint32_t>& bone_index)
{
   std::vector<std::uint32_t> result;
   result.reserve(bmap.size());

   for (const auto& b : bmap) {
      if (b >= bone_index.size()) {
         throw std::runtime_error{"Meshes bonemap is invalid!"};
      }

      result.push_back(bone_index[b]);
   }

   return result;
}

void fixup_texture_names(Material& material)
{
   auto& textures = material.textures;

   for (auto& texture : textures) {
      if (!texture.empty()) texture += ".tga"_sv;
   }
}

std::string create_coll_prim_name(Collision_flags flags, std::uint32_t index)
{
   std::string name{"p_-"};

   if (are_flags_set(flags, Collision_flags::soldier)) name += 's';
   if (are_flags_set(flags, Collision_flags::vehicle)) name += 'v';
   if (are_flags_set(flags, Collision_flags::building)) name += 'b';
   if (are_flags_set(flags, Collision_flags::terrain)) name += 't';
   if (are_flags_set(flags, Collision_flags::ordnance)) name += 'o';
   if (are_flags_set(flags, Collision_flags::flyer)) name += 'f';

   name += "-coll"_sv;
   name += std::to_string(index);

   return name;
}

Modl_section create_section_from(const Bone& bone, std::uint32_t index)
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

Modl_section create_section_from(const Model& model, std::uint32_t index,
                                 const std::vector<std::uint32_t>& bone_index)
{
   Modl_section section;

   section.type = Model_type::fixed;
   section.index = index;
   section.name = "mesh_"s + std::to_string(index);
   section.parent = model.parent;

   section.material = model.material;
   fixup_texture_names(*section.material);

   section.strips = strips_to_msh_fmt(model.strips);
   section.vertices = model.vertices;
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
      section.bone_map = remap_bonemap(model.bone_map, bone_index);
   }

   return section;
}

Modl_section create_section_from(const Shadow& shadow, std::uint32_t index,
                                 const std::vector<std::uint32_t>& bone_index)
{
   Modl_section section;

   section.type = Model_type::fixed;
   section.index = index;
   section.name = "sv_"s + std::to_string(index);
   section.parent = shadow.parent;

   section.strips = strips_to_msh_fmt(shadow.strips);
   section.vertices = shadow.vertices;

   if (!shadow.skin.empty()) {
      section.skin = shadow.skin;
      section.type = Model_type::skin;
   }
   if (!shadow.bone_map.empty()) {
      section.bone_map = remap_bonemap(shadow.bone_map, bone_index);
   }

   return section;
}

Modl_section create_section_from(const Collsion_mesh& collision, std::uint32_t index)
{
   Modl_section section;

   section.type = Model_type::fixed;
   section.index = index;
   section.name = "collision_"s + std::to_string(index);
   section.parent = collision.parent;

   section.strips = strips_to_msh_fmt(collision.strips);
   section.vertices = collision.vertices;

   return section;
}

Modl_section create_section_from(const Collision_primitive& primitive,
                                 std::uint32_t index)
{
   Modl_section section;

   section.type = Model_type::null;
   section.index = index;
   section.name = create_coll_prim_name(primitive.flags, index);
   section.parent = primitive.parent;
   section.translation = primitive.position;
   section.rotation = primitive.rotation;
   section.collision = {primitive.type, primitive.size};

   return section;
}

std::vector<Modl_section> create_modl_sections(
   std::vector<Bone> bones, std::vector<Model> models, std::vector<Shadow> shadows,
   std::vector<Collsion_mesh> collision_meshes,
   std::vector<Collision_primitive> collision_primitives)
{
   std::vector<Modl_section> sections;
   sections.reserve(bones.size() + models.size() + shadows.size());

   std::uint32_t model_index = 1;

   const auto bone_index = sort_bones(bones);

   const auto create_section_static = [&sections, &model_index](auto& item) {
      sections.emplace_back(create_section_from(item, model_index));

      model_index += 1;
   };

   const auto create_section_mesh = [&sections, &model_index, &bone_index](auto& item) {
      sections.emplace_back(create_section_from(item, model_index, bone_index));

      model_index += 1;
   };

   std::for_each(std::begin(bones), std::end(bones), create_section_static);
   std::for_each(std::begin(models), std::end(models), create_section_mesh);
   std::for_each(std::begin(shadows), std::end(shadows), create_section_mesh);
   std::for_each(std::begin(collision_meshes), std::end(collision_meshes),
                 create_section_static);
   std::for_each(std::begin(collision_primitives), std::end(collision_primitives),
                 create_section_static);

   return sections;
}

Ucfb_builder create_posl_chunk(const std::vector<glm::vec3>& vertices)
{
   Ucfb_builder posl{"POSL"_mn};
   posl.write(static_cast<std::uint32_t>(vertices.size()));

   for (const auto& v : vertices) {
      posl.write_multiple(v.x, v.y, v.z);
   }

   return posl;
}

Ucfb_builder create_wght_chunk(const std::vector<std::uint8_t>& skin)
{
   Ucfb_builder wght{"WGHT"_mn};
   wght.write(static_cast<std::uint32_t>(skin.size()));

   for (const auto& weight : skin) {
      wght.write_multiple(static_cast<std::uint32_t>(weight), 1.0f, 0ui32, 0.0f, 0ui32,
                          0.0f, 0ui32, 0.0f);
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

Ucfb_builder create_uv0l_chunk(const std::vector<glm::vec2>& texture_coords)
{
   Ucfb_builder clrl{"UV0L"_mn};
   clrl.write(static_cast<std::uint32_t>(texture_coords.size()));

   for (const auto& uv : texture_coords) {
      clrl.write_multiple(uv.x, uv.y);
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

   if (section.vertices) segm.add_child(create_posl_chunk(*section.vertices));
   if (section.skin) segm.add_child(create_wght_chunk(*section.skin));
   if (section.normals) segm.add_child(create_nrml_chunk(*section.normals));
   if (section.colours) segm.add_child(create_clrl_chunk(*section.colours));
   if (section.texture_coords) segm.add_child(create_uv0l_chunk(*section.texture_coords));
   if (section.strips) segm.add_child(create_strp_chunk(*section.strips));

   return segm;
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

   geom.add_child(create_segm_chunk(section));
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
   data.write_multiple(1.0f, 1.0f, 1.0f, 1.0f);
   data.write_multiple(material.colour.r, material.colour.g, material.colour.b,
                       material.colour.a);
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
   this->_shadows = other._shadows;
   this->_collision_meshes = other._collision_meshes;
   this->_collision_primitives = other._collision_primitives;

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

void Builder::add_shadow(Shadow shadow)
{
   _shadows.emplace_back(std::move(shadow));
}

void Builder::add_collision_mesh(Collsion_mesh collision_mesh)
{
   _collision_meshes.emplace_back(std::move(collision_mesh));
}

void Builder::add_collision_primitive(Collision_primitive primitive)
{
   _collision_primitives.emplace_back(std::move(primitive));
}

void Builder::set_bbox(const Bbox& bbox) noexcept
{
   std::lock_guard<tbb::spin_mutex> bbox_lock{_bbox_mutex};
   _bbox = bbox;
}

void Builder::save(const std::string& name, File_saver& file_saver) const
{
   auto msh_file = create_msh_file(
      create_modl_sections(downgrade_concurrent_vector(_bones),
                           downgrade_concurrent_vector(_models),
                           downgrade_concurrent_vector(_shadows),
                           downgrade_concurrent_vector(_collision_meshes),
                           downgrade_concurrent_vector(_collision_primitives)),
      get_bbox(), name);

   file_saver.save_file(std::move(msh_file), name + ".msh"s, "msh"s);
}

Bbox Builder::get_bbox() const noexcept
{
   std::lock_guard<tbb::spin_mutex> bbox_lock{_bbox_mutex};
   return _bbox;
}

void save_all(File_saver& file_saver, const Builders_map& builders)
{
   const auto functor = [&file_saver](const std::pair<std::string, Builder>& builder) {
      builder.second.save(builder.first, file_saver);
   };

   tbb::parallel_for_each(builders, functor);
}
}