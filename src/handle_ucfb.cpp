
#include "chunk_processor.hpp"

#include "tbb/parallel_for_each.h"

#include <vector>

void handle_ucfb(Ucfb_reader chunk, const App_options& app_options,
                 File_saver& file_saver)
{

   std::vector<Ucfb_reader> children;
   children.reserve(32);

   while (chunk) children.emplace_back(chunk.read_child());

   msh::Builders_map msh_builders;

   const auto processor = [&app_options, &file_saver,
                           &msh_builders](const Ucfb_reader& child) {
      process_chunk(child, app_options, file_saver, msh_builders);
   };

   tbb::parallel_for_each(children, processor);

   msh::save_all(file_saver, msh_builders, app_options.output_game_version());
}