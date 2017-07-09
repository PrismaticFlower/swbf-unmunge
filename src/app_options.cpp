#include "app_options.hpp"
#include "string_helpers.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace std::literals;

namespace {

std::stringstream create_arg_stream(int argc, char* argv[])
{
   std::stringstream arg_stream;

   for (auto i = 1; i < argc; ++i) {
      arg_stream << std::quoted(argv[i]);
   }

   return arg_stream;
}

auto read_filesystem_path(std::istream& istream)
{
   std::string str;
   istream >> std::quoted(str);

   return std::experimental::filesystem::path{str};
}
}

const auto fileinput_opt_description{R"(Set the input file to operate on.)"_sv};

const auto image_opt_description{
   R"(Set the output image format for textures. Can be 'tga', 'png' or 'dds'.)"_sv};

App_options::App_options()
{
   using Istr = std::istream;

   _options = {{"-file"s, [this](Istr& istr) { _file_path = read_filesystem_path(istr); },
                fileinput_opt_description}};
}

App_options::App_options(int argc, char* argv[]) : App_options()
{

   auto arg_stream = create_arg_stream(argc, argv);

   while (arg_stream) {
      std::string arg;
      arg_stream >> std::quoted(arg);

      const auto handler = find_option_handler(arg);

      if (handler) (*handler)(arg_stream);
   }
}

auto App_options::input_file() const noexcept
   -> const std::experimental::filesystem::path&
{
   return _file_path;
}

void App_options::print_arguments(std::ostream& ostream) noexcept
{
   ostream << '\n';

   for (const auto& option : _options) {
      ostream << ' ' << option.name << ' ';
      ostream.write(option.description.data(), option.description.length());
      ostream << '\n';
   }

   ostream << '\n';
}

auto App_options::find_option_handler(std::string_view name) noexcept
   -> App_options::Option_handler*
{
   const auto result =
      std::find_if(std::begin(_options), std::end(_options),
                   [name](const Option& option) { return (option.name == name); });

   if (result == std::end(_options)) return nullptr;

   return &result->handler;
}
