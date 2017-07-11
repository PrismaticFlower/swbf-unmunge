#include "chunk_headers.hpp"
#include "chunk_processor.hpp"
#include "type_pun.hpp"

#include "tbb/task_group.h"

void handle_lvl_child(Ucfb_reader lvl_child, const App_options& app_options,
                      File_saver& file_saver, tbb::task_group& tasks,
                      msh::Builders_map& msh_builders)
{
   while (lvl_child) {
      const auto child = lvl_child.read_child();

      const auto task = [child, &app_options, &file_saver, &tasks, &msh_builders] {
         process_chunk(child, app_options, file_saver, tasks, msh_builders);
      };

      tasks.run(task);
   }
}
