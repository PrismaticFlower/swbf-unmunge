#include "chunk_headers.hpp"
#include "chunk_processor.hpp"
#include "type_pun.hpp"
#include "ucfb_reader.hpp"

#include "tbb/task_group.h"

#include <cstdint>

void handle_ucfb(Ucfb_reader chunk, const App_options& app_options,
                 File_saver& file_saver, tbb::task_group& tasks,
                 msh::Builders_map& msh_builders)
{
   while (chunk) {
      const auto child = chunk.read_child();

      const auto task = [child, &app_options, &file_saver, &tasks, &msh_builders] {
         process_chunk(child, app_options, file_saver, tasks, msh_builders);
      };

      tasks.run(task);
   }
}