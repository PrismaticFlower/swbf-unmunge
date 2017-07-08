
#include "chunk_processor.hpp"
#include "chunk_handlers.hpp"
#include "chunk_headers.hpp"
#include "file_saver.hpp"
#include "magic_number.hpp"
#include "type_pun.hpp"

#include "tbb/task_group.h"

using namespace std::literals;

void process_chunk(const chunks::Unknown& chunk, File_saver& file_saver,
                   tbb::task_group& tasks, msh::Builders_map& msh_builders)
{
   if (chunk.mn == "ucfb"_mn) {
      tasks.run([&] {
         handle_ucfb(view_type_as<chunks::Ucfb>(chunk), file_saver, tasks, msh_builders);
      });
   }
   else if (chunk.mn == "lvl_"_mn) {
      tasks.run([&] {
         handle_lvl_child(view_type_as<chunks::Child_lvl>(chunk), file_saver, tasks,
                          msh_builders);
      });
   }
   // Object chunks
   else if (chunk.mn == "entc"_mn) {
      tasks.run([&] {
         handle_object(view_type_as<chunks::Object>(chunk), file_saver,
                       "GameObjectClass"s);
      });
   }
   else if (chunk.mn == "expc"_mn) {
      tasks.run([&] {
         handle_object(view_type_as<chunks::Object>(chunk), file_saver,
                       "ExplosionClass"s);
      });
   }
   else if (chunk.mn == "ordc"_mn) {
      tasks.run([&] {
         handle_object(view_type_as<chunks::Object>(chunk), file_saver, "OrdnanceClass"s);
      });
   }
   else if (chunk.mn == "wpnc"_mn) {
      tasks.run([&] {
         handle_object(view_type_as<chunks::Object>(chunk), file_saver, "WeaponClass"s);
      });
   }
   // Config chunks
   else if (chunk.mn == "fx__"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".fx", "effects");
      });
   }
   else if (chunk.mn == "sky_"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".sky", "world");
      });
   }
   else if (chunk.mn == "prp_"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".prp", "world",
                       true);
      });
   }
   else if (chunk.mn == "bnd_"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".bnd", "world",
                       true);
      });
   }
   else if (chunk.mn == "lght"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".light",
                       "world");
      });
   }
   else if (chunk.mn == "port"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".pvs", "world");
      });
   }
   else if (chunk.mn == "path"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".pth", "world");
      });
   }
   else if (chunk.mn == "comb"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".combo",
                       "combos");
      });
   }
   else if (chunk.mn == "sanm"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".sanm",
                       "config");
      });
   }
   else if (chunk.mn == "hud_"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".hud", "config");
      });
   }
   else if (chunk.mn == "load"_mn) {
      tasks.run([&] {
         handle_config(view_type_as<chunks::Config>(chunk), file_saver, ".cfg", "load");
      });
   }
   // Texture chunks
   else if (chunk.mn == "tex_"_mn) {
      tasks.run(
         [&] { handle_texture(view_type_as<chunks::Texture>(chunk), file_saver); });
   }
   // World chunks
   else if (chunk.mn == "wrld"_mn) {
      tasks.run(
         [&] { handle_world(view_type_as<chunks::World>(chunk), tasks, file_saver); });
   }
   else if (chunk.mn == "plan"_mn) {
      tasks.run(
         [&] { handle_planning(view_type_as<chunks::Planning>(chunk), file_saver); });
   }
   else if (chunk.mn == "plnp"_mn) {
      return;
   }
   else if (chunk.mn == "PATH"_mn) {
      tasks.run([&] { handle_path(view_type_as<chunks::Path>(chunk), file_saver); });
   }
   else if (chunk.mn == "tern"_mn) {
      tasks.run(
         [&] { handle_terrain(view_type_as<chunks::Terrain>(chunk), file_saver); });
   }
   // Model chunks
   else if (chunk.mn == "skel"_mn) {
      tasks.run(
         [&] { handle_skeleton(view_type_as<chunks::Skeleton>(chunk), msh_builders); });
   }
   else if (chunk.mn == "modl"_mn) {
      tasks.run(
         [&] { handle_model(view_type_as<chunks::Model>(chunk), msh_builders, tasks); });
   }
   else if (chunk.mn == "gmod"_mn) {
      return;
   }
   else if (chunk.mn == "coll"_mn) {
      tasks.run(
         [&] { handle_collision(view_type_as<chunks::Collision>(chunk), msh_builders); });
   }
   else if (chunk.mn == "prim"_mn) {
      tasks.run([&] {
         handle_primitives(view_type_as<chunks::Primitives>(chunk), msh_builders);
      });
   }
   // Misc chunks
   else if (chunk.mn == "Locl"_mn) {
      tasks.run([&] {
         handle_localization(view_type_as<chunks::Localization>(chunk), tasks,
                             file_saver);
      });
   }
   else {
      tasks.run([&] { handle_unknown(chunk, file_saver); });
   }
}
