#pragma once

#include "file_saver.hpp"
#include "ucfb_builder.hpp"

#include <experimental/filesystem>

void assemble_chunks(std::experimental::filesystem::path directory,
                     File_saver& file_saver);