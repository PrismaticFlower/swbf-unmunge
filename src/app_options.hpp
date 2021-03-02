#pragma once

#include "bit_flags.hpp"

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

enum class Tool_mode { extract, explode, assemble };

enum class Image_format { tga, png, dds };

enum class Model_format { msh, gltf2 };

enum class Model_discard_flags { none = 0b0, lod = 0b1, collision = 0b10, all = 0b11 };

constexpr bool marked_as_enum_flag(Model_discard_flags) noexcept
{
   return true;
}

enum class Game_version { swbf_ii, swbf };

enum class Input_platform { pc, ps2, xbox };

class App_options {
public:
   App_options(const App_options&) = delete;
   App_options& operator=(const App_options&) = delete;
   App_options(App_options&&) = delete;
   App_options& operator=(App_options&&) = delete;

   App_options(const int argc, char* argv[]);

   auto input_files() const noexcept -> const std::vector<std::string>&;

   Tool_mode tool_mode() const noexcept;

   Game_version game_version() const noexcept;

   Game_version output_game_version() const noexcept;

   Image_format image_save_format() const noexcept;

   Model_format model_format() const noexcept;

   Model_discard_flags model_discard_flags() const noexcept;

   Input_platform input_platform() const noexcept;

   std::string user_string_dict() const noexcept;

   std::string folder() const noexcept;

   std::string gen_string_dict() const noexcept;

   bool verbose() const noexcept;

   void print_arguments(std::ostream& ostream) noexcept;

private:
   App_options();

   using Option_handler = std::function<void(std::istream&)>;

   struct Option {
      std::string name;
      Option_handler handler;
      std::string_view description;
   };

   auto find_option_handler(std::string_view name) noexcept -> Option_handler*;

   std::vector<Option> _options;

   std::vector<std::string> _input_files;
   Tool_mode _tool_mode = Tool_mode::extract;
   Game_version _game_version = Game_version::swbf_ii;
   Game_version _output_game_version = Game_version::swbf_ii;
   Image_format _img_save_format = Image_format::tga;
   Model_format _model_format = Model_format::msh;
   std::string _user_string_dict;
   std::string _gen_string_dict;
   std::string _folder;
   Model_discard_flags _model_discard_flags = Model_discard_flags::none;
   Input_platform _input_platform = Input_platform::pc;
   bool _verbose = false;
};

// extern bool pre_processing;
bool get_pre_processing_global();

void set_pre_processing_global(bool value);

