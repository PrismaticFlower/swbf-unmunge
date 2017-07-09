#pragma once

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

class App_options {
public:
   App_options();

   App_options(const int argc, char* argv[]);

   auto input_file() const noexcept -> const std::experimental::filesystem::path&;

   void print_arguments(std::ostream& ostream) noexcept;

private:
   using Option_handler = std::function<void(std::istream&)>;

   struct Option {
      std::string name;
      Option_handler handler;
      std::string_view description;
   };

   auto find_option_handler(std::string_view name) noexcept -> Option_handler*;

   std::vector<Option> _options;

   std::experimental::filesystem::path _file_path;
};