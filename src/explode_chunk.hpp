#pragma once

#include "file_saver.hpp"
#include "ucfb_reader.hpp"

#include <cstddef>

void explode_chunk(Ucfb_reader chunk, File_saver& file_saver,
                   const std::size_t index = 0);