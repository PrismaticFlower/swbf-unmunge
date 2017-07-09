#pragma once

#include "app_options.hpp"
#include "chunk_headers.hpp"
#include "msh_builder.hpp"

#include <optional>
#include <string>

class File_saver;
namespace tbb {
class task_group;
}

void handle_unknown(const chunks::Unknown& chunk, File_saver& file_saver,
                    std::optional<std::string> file_name = {});

void handle_ucfb(const chunks::Ucfb& chunk, const App_options& app_options,
                 File_saver& file_saver, tbb::task_group& tasks,
                 msh::Builders_map& msh_builders);

void handle_lvl_child(const chunks::Child_lvl& chunk, const App_options& app_options,
                      File_saver& file_saver, tbb::task_group& tasks,
                      msh::Builders_map& msh_builders);

void handle_object(const chunks::Object& object, File_saver& file_saver,
                   std::string_view type);

void handle_config(const chunks::Config& chunk, File_saver& file_saver,
                   std::string_view file_name, std::string_view dir,
                   bool strings_are_hashed = false);

void handle_texture(const chunks::Texture& texture, File_saver& file_saver,
                    Image_format save_format);

void handle_world(const chunks::World& world, tbb::task_group& tasks,
                  File_saver& file_saver);

void handle_planning(const chunks::Planning& planning, File_saver& file_saver);

void handle_path(const chunks::Path& path, File_saver& file_saver);

void handle_localization(const chunks::Localization& locl, tbb::task_group& tasks,
                         File_saver& file_saver);

void handle_terrain(const chunks::Terrain& terr, File_saver& file_saver);

void handle_model(const chunks::Model& model, msh::Builders_map& builders,
                  tbb::task_group& tasks);

void handle_skeleton(const chunks::Skeleton& skel, msh::Builders_map& builders);

void handle_collision(const chunks::Collision& coll, msh::Builders_map& builders);

void handle_primitives(const chunks::Primitives& prim, msh::Builders_map& builders);
