#include "chunk_processor.hpp"

#include "tbb/parallel_for_each.h"

#include <utility>
#include <vector>

void handle_lvl_child(Ucfb_reader lvl_child, const App_options& app_options,
                      File_saver& file_saver)
{
   lvl_child.consume(4); // lvl name hash
   lvl_child.consume(4); // lvl size left

   std::vector<std::pair<Ucfb_reader, Ucfb_reader>> children_parents;
   children_parents.reserve(32);

   while (lvl_child) children_parents.emplace_back(lvl_child.read_child(), lvl_child);

   msh::Builders_map msh_builders;

   const auto processor = [&app_options, &file_saver,
                           &msh_builders](const auto& child_parent) {
      process_chunk(child_parent.first, child_parent.second, app_options, file_saver,
                    msh_builders);
   };

   tbb::parallel_for_each(children_parents, processor);

   msh::save_all(file_saver, msh_builders, app_options.output_game_version());
}
