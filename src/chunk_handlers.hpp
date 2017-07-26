#pragma once

#include "app_options.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include <optional>
#include <string>

class File_saver;
namespace tbb {
class task_group;
}

void handle_unknown(Ucfb_reader chunk, File_saver& file_saver,
                    std::optional<std::string> file_name = {});

void handle_ucfb(Ucfb_reader chunk, const App_options& app_options,
                 File_saver& file_saver, tbb::task_group& tasks,
                 msh::Builders_map& msh_builders);

void handle_lvl_child(Ucfb_reader lvl_child, const App_options& app_options,
                      File_saver& file_saver, tbb::task_group& tasks,
                      msh::Builders_map& msh_builders);

void handle_object(Ucfb_reader object, File_saver& file_saver, std::string_view type);

void handle_config(Ucfb_reader config, File_saver& file_saver, std::string_view file_name,
                   std::string_view dir, bool strings_are_hashed = false);

void handle_texture(Ucfb_reader texture, File_saver& file_saver,
                    Image_format save_format);

void handle_world(Ucfb_reader world, tbb::task_group& tasks, File_saver& file_saver);

void handle_planning(Ucfb_reader planning, File_saver& file_saver);

void handle_planning_swbf1(Ucfb_reader planning, File_saver& file_saver);

void handle_path(Ucfb_reader path, File_saver& file_saver);

void handle_localization(Ucfb_reader localization, tbb::task_group& tasks,
                         File_saver& file_saver);

void handle_terrain(Ucfb_reader terrain, File_saver& file_saver);

void handle_model(Ucfb_reader model, msh::Builders_map& builders);

void handle_model_ps2(Ucfb_reader model, msh::Builders_map& builders);

void handle_skeleton(Ucfb_reader skeleton, msh::Builders_map& builders);

void handle_collision(Ucfb_reader collision, msh::Builders_map& builders);

void handle_primitives(Ucfb_reader primitives, msh::Builders_map& builders);
