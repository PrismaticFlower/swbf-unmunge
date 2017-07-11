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

std::istream& operator>>(std::istream& istream, Game_version& game_version)
{
   std::string str;
   istream >> std::quoted(str);

   if (str == "swbf_ii"_sv) {
      game_version = Game_version::swbf_ii;
   }
   else if (str == "swbf"_sv) {
      game_version = Game_version::swbf;
   }
   else {
      throw std::invalid_argument{"Invalid game version specified."};
   }

   return istream;
}

std::istream& operator>>(std::istream& istream, Image_format& image_type)
{
   std::string str;
   istream >> std::quoted(str);

   if (str == "tga"_sv) {
      image_type = Image_format::tga;
   }
   else if (str == "png"_sv) {
      image_type = Image_format::png;
   }
   else if (str == "dds"_sv) {
      image_type = Image_format::dds;
   }
   else {
      throw std::invalid_argument{"Invalid image format specified."};
   }

   return istream;
}

std::istream& operator>>(std::istream& istream, Input_platform& platform)
{
   std::string str;
   istream >> std::quoted(str);

   if (str == "pc"_sv) {
      platform = Input_platform::pc;
   }
   else if (str == "ps2"_sv) {
      platform = Input_platform::ps2;
   }
   else if (str == "xbox"_sv) {
      platform = Input_platform::xbox;
   }
   else {
      throw std::invalid_argument{"Invalid input platform specified."};
   }

   return istream;
}
}

const auto fileinput_opt_description{
   R"(<filepath> Set the input file to operate on.)"_sv};

const auto game_ver_opt_description{
   R"(<version> Set the game version of the input file. Can be 'swbf_ii' or 'swbf. Default is 'swbf_ii'.)"_sv};

const auto image_opt_description{
   R"(<format> Set the output image format for textures. Can be 'tga', 'png' or 'dds'. Default is 'tga'.)"_sv};

const auto input_plat_opt_description{
   R"(<format> Set the platform the input file was munged for. Can be 'pc', 'ps2' or 'xbox'. Default is 'pc'.)"_sv};

App_options::App_options()
{
   using Istr = std::istream;

   _options = {{"-file"s, [this](Istr& istr) { _file_path = read_filesystem_path(istr); },
                fileinput_opt_description},
               {"-version"s, [this](Istr& istr) { istr >> _game_version; },
                game_ver_opt_description},
               {"-imgfmt"s, [this](Istr& istr) { istr >> _img_save_format; },
                image_opt_description},
               {"-platform"s, [this](Istr& istr) { istr >> _input_platform; },
                input_plat_opt_description}

   };
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

Game_version App_options::game_version() const noexcept
{
   return _game_version;
}

Image_format App_options::image_save_format() const noexcept
{
   return _img_save_format;
}

Input_platform App_options::input_platform() const noexcept
{
   return _input_platform;
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
