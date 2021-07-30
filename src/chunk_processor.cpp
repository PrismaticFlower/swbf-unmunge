
#include "chunk_processor.hpp"
#include "chunk_handlers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "string_helpers.hpp"
#include "synced_cout.hpp"
#include "type_pun.hpp"

#include "tbb/task_group.h"

#include <algorithm>
#include <initializer_list>
#include <tuple>
#include <unordered_map>

using namespace std::literals;

namespace {

struct Args_pack {
   Ucfb_reader chunk;
   Ucfb_reader parent_reader;
   const App_options& app_options;
   File_saver& file_saver;
   const Swbf_fnv_hashes& swbf_hashes;
   model::Models_builder& models_builder;
};

void ignore_chunk(Args_pack){};

class Chunk_processor_map {
public:
   using Processor_func = void (*)(Args_pack);

   using Key_value_pair =
      std::pair<const Magic_number,
                std::tuple<Input_platform, Game_version, Processor_func>>;

   Chunk_processor_map(std::initializer_list<Key_value_pair> init_list) noexcept
      : _processors{init_list}
   {
   }

   // Looksup a chunk processor by performing the following.
   //
   // First all the processors for a magic number are fetched. If none exist nullptr is
   // returned.
   //
   // Then an exact match for the platform and game version is searched for, if it is
   // found it's returned.
   //
   // Else a platform match is searched for if it is found it is returned.
   //
   // Else if no platform match was found a game version match is searched for, if it is
   // found then it is returned.
   //
   // Finally if no ideal match can be found the first processor is returned.
   Processor_func lookup(Magic_number mn, Input_platform platform,
                         Game_version version) const noexcept
   {
      const auto range = _processors.equal_range(mn);

      if (range.first == range.second) return {};

      const auto exact_match = find_exact_match(range, platform, version);

      if (exact_match != range.second) {
         return std::get<Processor_func>(exact_match->second);
      }

      const auto platform_match = find_platform_match(range, platform);

      if (platform_match != range.second) {
         return std::get<Processor_func>(platform_match->second);
      }

      const auto version_match = find_game_match(range, version);

      if (version_match != range.second) {
         return std::get<Processor_func>(version_match->second);
      }

      return std::get<Processor_func>(range.first->second);
   }

private:
   using Map_type =
      std::unordered_multimap<Magic_number,
                              std::tuple<Input_platform, Game_version, Processor_func>>;
   using Const_iterator = Map_type::const_iterator;
   using Handler_range = std::pair<Const_iterator, Const_iterator>;

   static Const_iterator find_exact_match(const Handler_range range,
                                          Input_platform platform, Game_version version)
   {
      return std::find_if(range.first, range.second,
                          [platform, version](const Key_value_pair& pair) {
                             return (platform == std::get<Input_platform>(pair.second) &&
                                     version == std::get<Game_version>(pair.second));
                          });
   }

   static Const_iterator find_platform_match(const Handler_range range,
                                             Input_platform platform)
   {
      return std::find_if(range.first, range.second,
                          [platform](const Key_value_pair& pair) {
                             return (platform == std::get<Input_platform>(pair.second));
                          });
   }

   static Const_iterator find_game_match(const Handler_range range, Game_version version)
   {
      return std::find_if(range.first, range.second,
                          [version](const Key_value_pair& pair) {
                             return (version == std::get<Game_version>(pair.second));
                          });
   }

   Map_type _processors;
};

const auto chunk_processors = Chunk_processor_map{
   // Parent Chunks
   {"ucfb"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_ucfb(args.chunk, args.app_options, args.file_saver, args.swbf_hashes);
     }}},

   {"lvl_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_lvl_child(args.chunk, args.app_options, args.file_saver, args.swbf_hashes);
     }}},

   // Class Chunks
   {"entc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, args.swbf_hashes, "GameObjectClass"sv);
     }}},
   {"expc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, args.swbf_hashes, "ExplosionClass"sv);
     }}},
   {"ordc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, args.swbf_hashes, "OrdnanceClass"sv);
     }}},
   {"wpnc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, args.swbf_hashes, "WeaponClass"sv);
     }}},

   // Config chunks
   {"fx__"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".fx"sv,
                      "effects"sv);
     }}},
   {"sky_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".sky"sv, "world"sv);
     }}},
   {"prp_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".prp"sv, "world"sv,
                      true);
     }}},
   {"bnd_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".bnd"sv, "world"sv,
                      true);
     }}},
   {"lght"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".lgt"sv, "world"sv);
     }}},
   {"port"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".pvs"sv, "world"sv);
     }}},
   {"path"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".pth"sv, "world"sv);
     }}},
   {"comb"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".combo"sv,
                      "combos"sv);
     }}},
   {"sanm"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".sanm"sv,
                      "config"sv);
     }}},
   {"hud_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".hud"sv,
                      "config"sv);
     }}},
   {"load"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".cfg"sv,
                      "config"sv);
     }}},
   {"mcfg"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".mcfg"sv,
                      "config"sv, true);
     }}},
   {"snd_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".snd"sv, "config"sv,
                      true);
     }}},
   {"mus_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".mus"sv, "config"sv,
                      true);
     }}},
   {"ffx_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, args.swbf_hashes, ".ffx"sv, "config"sv,
                      true);
     }}},

   // Texture chunks
   {"tex_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_texture(args.chunk, args.file_saver, args.app_options.image_save_format(),
                       args.app_options.model_format());
     }}},
   {"tex_"_mn,
    {Input_platform::ps2, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_texture_ps2(args.chunk, args.parent_reader, args.file_saver,
                           args.app_options.image_save_format(),
                           args.app_options.model_format());
     }}},
   {"tex_"_mn,
    {Input_platform::xbox, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_texture_xbox(args.chunk, args.file_saver,
                            args.app_options.image_save_format(),
                            args.app_options.model_format());
     }}},
   // World chunks
   {"wrld"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_world(args.chunk, args.file_saver, args.swbf_hashes);
     }}},
   {"plan"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_planning(args.chunk, args.file_saver); }}},
   {"plan"_mn,
    {Input_platform::pc, Game_version::swbf,
     [](Args_pack args) { handle_planning_swbf1(args.chunk, args.file_saver); }}},
   {"PATH"_mn,
    {Input_platform::pc, Game_version::swbf,
     [](Args_pack args) { handle_path(args.chunk, args.file_saver); }}},
   {"tern"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_terrain(args.chunk, args.app_options.output_game_version(),
                       args.file_saver);
     }}},
   // Model chunks
   {"skel"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_skeleton(args.chunk, args.models_builder); }}},
   {"modl"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_model(args.chunk, args.models_builder); }}},
   {"modl"_mn,
    {Input_platform::xbox, Game_version::swbf_ii,
     [](Args_pack args) { handle_model_xbox(args.chunk, args.models_builder); }}},
   {"modl"_mn,
    {Input_platform::ps2, Game_version::swbf_ii,
     [](Args_pack args) { handle_model_ps2(args.chunk, args.models_builder); }}},
   {"coll"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_collision(args.chunk, args.models_builder); }}},
   {"prim"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_primitives(args.chunk, args.models_builder); }}},
   {"CLTH"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_cloth(args.chunk, args.models_builder); }}},
   // Misc chunks
   {"Locl"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_localization(args.chunk, args.file_saver); }}},
   {"scr_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_script(args.chunk, args.file_saver); }}},
   {"SHDR"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_shader(args.chunk, args.file_saver); }}},
   {"font"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_font(args.chunk, args.file_saver); }}},
   {"zaa_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_zaabin(args.chunk, args.file_saver); }}},
   {"zaf_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_binary(args.chunk, args.file_saver, ".zafbin"sv); }}},

   // Ignored Chunks, for which we want no output at all.
   {"gmod"_mn, {Input_platform::pc, Game_version::swbf_ii, ignore_chunk}},
   {"plnp"_mn, {Input_platform::pc, Game_version::swbf_ii, ignore_chunk}},
};
}

void process_chunk(Ucfb_reader chunk, Ucfb_reader parent_reader,
                   const App_options& app_options, File_saver& file_saver,
                   const Swbf_fnv_hashes& swbf_hashes,
                   model::Models_builder& models_builder)
{
   const auto processor = chunk_processors.lookup(
      chunk.magic_number(), app_options.input_platform(), app_options.game_version());

   if (processor) {
      try {
         processor(
            {chunk, parent_reader, app_options, file_saver, swbf_hashes, models_builder});
      }
      catch (const std::exception& e) {
         synced_cout::print("Error: Exception occured while processing chunk.\n"
                            "   Type: "s,
                            view_object_as_string(chunk.magic_number()), "\n   Size: "s,
                            chunk.size(), "\n   Message: "s, e.what(), '\n');
      }
   }
   else {
      handle_unknown(chunk, file_saver);
   }
}
