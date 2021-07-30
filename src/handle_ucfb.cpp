
#include "chunk_processor.hpp"
#include "model_builder.hpp"
#include "swbf_fnv_hashes.hpp"

#include "tbb/parallel_for_each.h"

#include <vector>

void handle_ucfb(Ucfb_reader chunk, const App_options& app_options,
                 File_saver& file_saver, const Swbf_fnv_hashes& swbf_hashes)
{
   std::vector<std::pair<Ucfb_reader, Ucfb_reader>> children_parents;
   children_parents.reserve(32);

   while (chunk) children_parents.emplace_back(chunk.read_child(), chunk);

   model::Models_builder models_builder;

   const auto processor = [&app_options, &file_saver, &swbf_hashes,
                           &models_builder](const auto& child_parent) {
      process_chunk(child_parent.first, child_parent.second, app_options, file_saver,
                    swbf_hashes, models_builder);
   };

   tbb::parallel_for_each(children_parents, processor);

   models_builder.save_models(file_saver, app_options.output_game_version(),
                              app_options.model_format(),
                              app_options.model_discard_flags());
}
