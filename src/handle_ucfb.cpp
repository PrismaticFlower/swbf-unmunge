
#include "chunk_processor.hpp"

#include "tbb/parallel_for_each.h"

#include <vector>

void handle_ucfb(Ucfb_reader chunk, const App_options& app_options,
                 File_saver& file_saver)
{
   std::vector<std::pair<Ucfb_reader, Ucfb_reader>> children_parents;
   children_parents.reserve(32);

   while (chunk) children_parents.emplace_back(chunk.read_child(), chunk);

   msh::Builders_map msh_builders;

   const auto processor = [&app_options, &file_saver,
                           &msh_builders](const auto& child_parent) {
      process_chunk(child_parent.first, child_parent.second, app_options, file_saver,
                    msh_builders);
   };

   tbb::parallel_for_each(children_parents, processor);

   msh::save_all(file_saver, msh_builders, app_options.output_game_version());
}
