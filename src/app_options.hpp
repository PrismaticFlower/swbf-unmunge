#pragma once

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

enum class Image_format { tga, png, dds };

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

   Game_version game_version() const noexcept;

   Image_format image_save_format() const noexcept;

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
   Game_version _game_version;
   Image_format _img_save_format = Image_format::tga;
   Input_platform _input_platform;
   bool _verbose = false;
};