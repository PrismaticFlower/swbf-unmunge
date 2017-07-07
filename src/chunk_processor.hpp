#pragma once

#include"msh_builder.hpp"

namespace chunks { struct Unknown; }
class File_saver;
namespace tbb { class task_group; }

void process_chunk(const chunks::Unknown& chunk,
                   File_saver& file_saver,
                   tbb::task_group& tasks,
                   msh::Builders_map& msh_builders);