#pragma once

#include "file_saver.hpp"
#include "ucfb_builder.hpp"

#include <filesystem>

void assemble_chunks(std::filesystem::path directory, File_saver& file_saver);
