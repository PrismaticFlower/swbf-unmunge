#pragma once

#include "ucfb_reader.hpp"

namespace chunks {
struct Unknown;
}
namespace model {
class Models_builder;
}
class App_options;
class File_saver;
namespace tbb {
class task_group;
}

void process_chunk(Ucfb_reader chunk, Ucfb_reader parent_reader,
                   const App_options& app_options, File_saver& file_saver,
                   model::Models_builder& models_builder);
