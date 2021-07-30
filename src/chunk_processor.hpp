#pragma once

#include "swbf_fnv_hashes.hpp"
#include "ucfb_reader.hpp"

namespace model {
class Models_builder;
}
class App_options;
class File_saver;

void process_chunk(Ucfb_reader chunk, Ucfb_reader parent_reader,
                   const App_options& app_options, File_saver& file_saver,
                   const Swbf_fnv_hashes& swbf_hashes,
                   model::Models_builder& models_builder);
