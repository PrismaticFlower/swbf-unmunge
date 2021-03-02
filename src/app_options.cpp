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

std::string read_file_path(std::istream& istream)
{
   std::string str;
   istream >> std::quoted(str);

   return str;
}

auto append_file_list(std::istream& istream, std::vector<std::string>& out)
{
   std::string list;
   istream >> std::quoted(list);

   std::string_view view{list};

   constexpr auto delimiter = ';';

   for (auto offset = view.find(delimiter); (offset != 0 && offset != view.npos);
        offset = view.find(delimiter)) {
      out.emplace_back(view.substr(0, offset));

      view.remove_prefix(offset + 1);
   }

   if (!view.empty()) out.emplace_back(view);
}

std::istream& operator>>(std::istream& istream, Tool_mode& mode)
{
   std::string str;
   istream >> std::quoted(str);

   if (str == "extract"sv) {
      mode = Tool_mode::extract;
   }
   else if (str == "explode"sv) {
      mode = Tool_mode::explode;
   }
   else if (str == "assemble"sv) {
      mode = Tool_mode::assemble;
   }
   else {
      throw std::invalid_argument{"Invalid tool mode specified."};
   }

   return istream;
}

std::istream& operator>>(std::istream& istream, Game_version& game_version)
{
   std::string str;
   istream >> std::quoted(str);

   if (str == "swbf_ii"sv) {
      game_version = Game_version::swbf_ii;
   }
   else if (str == "swbf"sv) {
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

   if (str == "tga"sv) {
      image_type = Image_format::tga;
   }
   else if (str == "png"sv) {
      image_type = Image_format::png;
   }
   else if (str == "dds"sv) {
      image_type = Image_format::dds;
   }
   else {
      throw std::invalid_argument{"Invalid image format specified."};
   }

   return istream;
}

std::istream& operator>>(std::istream& istream, Model_format& model_format)
{
   std::string str;
   istream >> std::quoted(str);

   if (str == "msh"sv) {
      model_format = Model_format::msh;
   }
   else if (str == "glTF"sv) {
      model_format = Model_format::gltf2;
   }
   else {
      throw std::invalid_argument{"Invalid image format specified."};
   }

   return istream;
}

std::istream& operator>>(std::istream& istream, Model_discard_flags& model_discard)
{
   std::string str;
   istream >> std::quoted(str);

   if (str == "none"sv) {
      model_discard = Model_discard_flags::none;
   }
   else if (str == "lod"sv) {
      model_discard = Model_discard_flags::lod;
   }
   else if (str == "collision"sv) {
      model_discard = Model_discard_flags::collision;
   }
   else if (str == "lod_collision"sv) {
      model_discard = Model_discard_flags::lod | Model_discard_flags::collision;
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

   if (str == "pc"sv) {
      platform = Input_platform::pc;
   }
   else if (str == "ps2"sv) {
      platform = Input_platform::ps2;
   }
   else if (str == "xbox"sv) {
      platform = Input_platform::xbox;
   }
   else {
      throw std::invalid_argument{"Invalid input platform specified."};
   }

   return istream;
}
}

constexpr auto fileinput_opt_description{
   R"(<filepath> Specify an input file to operate on.)"sv};

constexpr auto files_opt_description{
   R"(<files> Specify a list of input files to operate, delimited by ';'.
   Example: "-files foo.lvl;bar.lvl")"sv};

constexpr auto game_ver_opt_description{
   R"(<version> Set the game version of the input file. Can be 'swbf_ii' or 'swbf. Default is 'swbf_ii'.)"sv};

constexpr auto gameout_ver_opt_description{
   R"(<version> Set the game version the output files will target. Can be 'swbf_ii' or 'swbf. Default is 'swbf_ii'.)"sv};

constexpr auto image_opt_description{
   R"(<format> Set the output image format for textures. Can be 'tga', 'png' or 'dds'. Default is 'tga'.)"sv};

constexpr auto model_format_opt_description{
   R"(<mode> Set the output storage format of extracted models. Can be 'msh' or 'glTF'. Default is 'msh'.)"sv};

constexpr auto model_discard_lod_opt_description{
   R"(<discard> Sets what to discard from extracted models before saving them to produce cleaner scenes.
   'none' (default) - Discard nothing.
   'lod' - Discard LOD copies of the model, leaving only the most detailed copy of the model.
   'collision' - Discard the model's collision information.
   'lod_collision' - Discard both the model's collision information and LOD copies.)"sv};

constexpr auto input_plat_opt_description{
   R"(<platform> Set the platform the input file was munged for. Can be 'pc', 'ps2' or 'xbox'. Default is 'pc'.)"sv};

constexpr auto verbose_opt_description{R"(Enable verbose output.)"sv};

constexpr auto string_dict_opt_description{
   R"(<dictionary_file> Specify a file of strings to be used in hash lookup; used in addition to the 
   program's built in string dictionary. File format is plain text, 1 line = 1 string.)"sv};

constexpr auto gen_dict_opt_description{
   R"(<dictionary_file> Save the hash lookup dictionary to the specified file.)"sv};

constexpr auto mode_opt_description{
   R"(<mode> Set the mode of operation for the tool. Can be 'extract', 'explode' or 'assemble'.
   'extract' (default) - Extract and "unmunge" the contents of the file.
   'explode' - Recursively explode the file's chunks into their hierarchies.
   'assemble' - Recursively assemble a previously exploded file. Input files will be treated as directories.)"sv};

constexpr auto folder_opt_description{
   R"(<folder> - process all .lvl files found under this folder.)"sv};

App_options::App_options()
{
   using Istr = std::istream;

   _options = {
      {"-file"s, [this](Istr& istr) { _input_files.emplace_back(read_file_path(istr)); },
       fileinput_opt_description},
      {"-files"s, [this](Istr& istr) { append_file_list(istr, _input_files); },
       files_opt_description},
      {"-version"s, [this](Istr& istr) { istr >> _game_version; },
       game_ver_opt_description},
      {"-outversion"s, [this](Istr& istr) { istr >> _output_game_version; },
       gameout_ver_opt_description},
      {"-imgfmt"s, [this](Istr& istr) { istr >> _img_save_format; },
       image_opt_description},
      {"-modelfmt"s, [this](Istr& istr) { istr >> _model_format; },
       model_format_opt_description},
      {"-modeldiscard"s, [this](Istr& istr) { istr >> _model_discard_flags; },
       model_discard_lod_opt_description},
      {"-platform"s, [this](Istr& istr) { istr >> _input_platform; },
       input_plat_opt_description},
      {"-verbose"s, [this](Istr&) { _verbose = true; }, verbose_opt_description},
      {"-string_dict"s, [this](Istr& istr) { _user_string_dict = read_file_path(istr); },
       string_dict_opt_description},
      {"-gen_string_dict"s, [this](Istr& istr) { _gen_string_dict = read_file_path(istr); },
       gen_dict_opt_description},
      {"-mode"s, [this](Istr& istr) { istr >> _tool_mode; }, mode_opt_description},
      {"-folder"s, [this](Istr& istr) { _folder = read_file_path(istr); },
       folder_opt_description}};
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

auto App_options::input_files() const noexcept -> const std::vector<std::string>&
{
   return _input_files;
}

Tool_mode App_options::tool_mode() const noexcept
{
   return _tool_mode;
}

Game_version App_options::game_version() const noexcept
{
   return _game_version;
}

Game_version App_options::output_game_version() const noexcept
{
   return _output_game_version;
}

Image_format App_options::image_save_format() const noexcept
{
   return _img_save_format;
}

Model_format App_options::model_format() const noexcept
{
   return _model_format;
}

Model_discard_flags App_options::model_discard_flags() const noexcept
{
   return _model_discard_flags;
}

Input_platform App_options::input_platform() const noexcept
{
   return _input_platform;
}

std::string App_options::user_string_dict() const noexcept
{
   return _user_string_dict;
}

std::string App_options::gen_string_dict() const noexcept
{
   return _gen_string_dict;
}

std::string App_options::folder() const noexcept
{
   return _folder;
}

bool App_options::verbose() const noexcept
{
   return _verbose;
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

namespace {

bool pre_processing_global = false;

}

bool get_pre_processing_global()
{
   return pre_processing_global;
}

void set_pre_processing_global(bool value)
{
   pre_processing_global = value;
}
