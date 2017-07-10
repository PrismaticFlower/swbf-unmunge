#pragma once

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

enum class Image_format { tga, png, dds };

enum class Game_version { swbf_ii, swbf };

class App_options {
public:
   App_options(const App_options&) = delete;
   App_options& operator=(const App_options&) = delete;
   App_options(App_options&&) = delete;
   App_options& operator=(App_options&&) = delete;

   App_options(const int argc, char* argv[]);

   auto input_file() const noexcept -> const std::experimental::filesystem::path&;

   Game_version game_version() const noexcept;

   Image_format image_save_format() const noexcept;

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

   std::experimental::filesystem::path _file_path;
   Game_version _game_version;
   Image_format _img_save_format = Image_format::tga;
};