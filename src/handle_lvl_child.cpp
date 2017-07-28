#include "chunk_processor.hpp"

#include "tbb/parallel_for_each.h"

#include <vector>

void handle_lvl_child(Ucfb_reader lvl_child, const App_options& app_options,
                      File_saver& file_saver)
{
   lvl_child.consume(4); // lvl name hash
   lvl_child.consume(4); // lvl size left

   std::vector<Ucfb_reader> children;
   children.reserve(32);

   while (lvl_child) children.emplace_back(lvl_child.read_child());

   msh::Builders_map msh_builders;

   const auto processor = [&app_options, &file_saver,
                           &msh_builders](const Ucfb_reader& child) {
      process_chunk(child, app_options, file_saver, msh_builders);
   };

   tbb::parallel_for_each(children, processor);

   msh::save_all(file_saver, msh_builders);
}
