
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
   const App_options& app_options;
   File_saver& file_saver;
   tbb::task_group& tasks;
   msh::Builders_map& msh_builders;
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
        handle_ucfb(args.chunk, args.app_options, args.file_saver, args.tasks,
                    args.msh_builders);
     }}},

   {"lvl_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_lvl_child(args.chunk, args.app_options, args.file_saver, args.tasks,
                         args.msh_builders);
     }}},

   // Class Chunks
   {"entc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, "GameObjectClass"_sv);
     }}},
   {"expc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, "ExplosionClass"_sv);
     }}},
   {"ordc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, "OrdnanceClass"_sv);
     }}},
   {"wpnc"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_object(args.chunk, args.file_saver, "WeaponClass"_sv);
     }}},

   // Config chunks
   {"fx__"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".fx"_sv, "effects"_sv);
     }}},
   {"sky_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".sky"_sv, "world"_sv);
     }}},
   {"prp_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".prp"_sv, "world"_sv);
     }}},
   {"bnd_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".bnd"_sv, "world"_sv);
     }}},
   {"lght"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".light"_sv, "world"_sv);
     }}},
   {"port"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".pvs"_sv, "world"_sv);
     }}},
   {"path"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".pth"_sv, "world"_sv);
     }}},
   {"comb"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".combo"_sv, "combos"_sv);
     }}},
   {"sanm"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".sanm"_sv, "config"_sv);
     }}},
   {"hud_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".hud"_sv, "config"_sv);
     }}},
   {"load"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_config(args.chunk, args.file_saver, ".cfg"_sv, "config"_sv);
     }}},

   // Texture chunks
   {"tex_"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_texture(args.chunk, args.file_saver, args.app_options.image_save_format());
     }}},
   {"tex_"_mn, {Input_platform::ps2, Game_version::swbf_ii, nullptr}},
   {"tex_"_mn, {Input_platform::xbox, Game_version::swbf_ii, nullptr}},

   // World chunks
   {"wrld"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_world(args.chunk, args.tasks, args.file_saver); }}},
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
     [](Args_pack args) { handle_terrain(args.chunk, args.file_saver); }}},
   // Model chunks
   {"skel"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_skeleton(args.chunk, args.msh_builders); }}},
   {"modl"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_model(args.chunk, args.msh_builders, args.tasks); }}},
   {"coll"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_collision(args.chunk, args.msh_builders); }}},
   {"prim"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) { handle_primitives(args.chunk, args.msh_builders); }}},

   // Misc chunks
   {"Locl"_mn,
    {Input_platform::pc, Game_version::swbf_ii,
     [](Args_pack args) {
        handle_localization(args.chunk, args.tasks, args.file_saver);
     }}},

   // Ignored Chunks, for which we want no output at all.
   {"gmod"_mn, {Input_platform::pc, Game_version::swbf_ii, ignore_chunk}},
   {"plnp"_mn, {Input_platform::pc, Game_version::swbf_ii, ignore_chunk}},
};
}

void process_chunk(Ucfb_reader chunk, const App_options& app_options,
                   File_saver& file_saver, tbb::task_group& tasks,
                   msh::Builders_map& msh_builders)
{
   const auto processor = chunk_processors.lookup(
      chunk.magic_number(), app_options.input_platform(), app_options.game_version());

   if (processor) {
      tasks.run([&, chunk, processor] {
         try {
            processor({chunk, app_options, file_saver, tasks, msh_builders});
         }
         catch (const std::exception& e) {
            synced_cout::print("Exception occured while processing chunk.\n"
                               "   Type: "s,
                               view_pod_as_string(chunk.magic_number()), "\n   Size: "s,
                               chunk.size(), "\n   Message: "s, e.what(), '\n');
         }
      });
   }
   else {
      tasks.run([&, chunk] { handle_unknown(chunk, file_saver); });
   }
}
