#pragma once

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

enum class Tool_mode { extract, explode, assemble };

enum class Image_format { tga, png, dds };

enum class Model_format { msh, gltf2 };

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

   Input_platform input_platform() const noexcept;

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
   Input_platform _input_platform = Input_platform::pc;
   bool _verbose = false;
};