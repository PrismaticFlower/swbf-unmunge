#pragma once

#include "app_options.hpp"
#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

#include <optional>
#include <string>

class File_saver;

void handle_unknown(Ucfb_reader chunk, File_saver& file_saver,
                    std::optional<std::string_view> file_name = {},
                    std::optional<std::string_view> file_extension = {});

void handle_ucfb(Ucfb_reader chunk, const App_options& app_options,
                 File_saver& file_saver);

void handle_lvl_child(Ucfb_reader lvl_child, const App_options& app_options,
                      File_saver& file_saver);

void handle_object(Ucfb_reader object, File_saver& file_saver, std::string_view type);

void handle_config(Ucfb_reader config, File_saver& file_saver, std::string_view file_name,
                   std::string_view dir, bool strings_are_hashed = false);

void handle_texture(Ucfb_reader texture, File_saver& file_saver,
                    Image_format save_format);

void handle_world(Ucfb_reader world, File_saver& file_saver);

void handle_planning(Ucfb_reader planning, File_saver& file_saver);

void handle_planning_swbf1(Ucfb_reader planning, File_saver& file_saver);

void handle_path(Ucfb_reader path, File_saver& file_saver);

void handle_localization(Ucfb_reader localization, File_saver& file_saver);

void handle_terrain(Ucfb_reader terrain, Game_version output_version,
                    File_saver& file_saver);

void handle_model(Ucfb_reader model, msh::Builders_map& builders);

void handle_model_ps2(Ucfb_reader model, msh::Builders_map& builders);

void handle_skeleton(Ucfb_reader skeleton, msh::Builders_map& builders);

void handle_collision(Ucfb_reader collision, msh::Builders_map& builders);

void handle_primitives(Ucfb_reader primitives, msh::Builders_map& builders);

void handle_cloth(Ucfb_reader cloth, msh::Builders_map& builders);

void handle_script(Ucfb_reader script, File_saver& file_saver);

void handle_shader(Ucfb_reader shader, File_saver& file_saver);

void handle_font(Ucfb_reader font, File_saver& file_saver);

void handle_binary(Ucfb_reader binary, File_saver& file_saver,
                   std::string_view extension);

void handle_zaabin(Ucfb_reader zaabin, File_saver& file_saver);
