#include"chunk_processor.hpp"
#include"chunk_headers.hpp"
#include"type_pun.hpp"

#include"tbb/task_group.h"

#include<cstdint>

void handle_ucfb(const chunks::Ucfb& chunk,
                 File_saver& file_saver,
                 tbb::task_group& tasks,
                 msh::Builders_map& msh_builders)
{
   std::uint32_t head = 0;
   const std::uint32_t end = chunk.size;

   while (head < end)
   {
      const auto& child = view_type_as<chunks::Unknown>(chunk.bytes[head]);

      const auto task = [&child, &file_saver, &tasks, &msh_builders]
      {
         process_chunk(child, file_saver, tasks, msh_builders);
      };

      tasks.run(task);

      head += (child.size + sizeof(chunks::Unknown));

      if (head % 4 != 0) head += (4 - (head % 4));
   }
}