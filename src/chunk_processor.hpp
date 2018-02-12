#pragma once

#include "msh_builder.hpp"
#include "ucfb_reader.hpp"

namespace chunks {
struct Unknown;
}
class App_options;
class File_saver;
namespace tbb {
class task_group;
}

void process_chunk(Ucfb_reader chunk, Ucfb_reader parent_reader,
                   const App_options& app_options, File_saver& file_saver,
                   msh::Builders_map& msh_builders);
