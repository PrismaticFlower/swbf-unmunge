#pragma once

#include "app_options.hpp"
#include "file_saver.hpp"

void save_image(std::string_view name, uint32_t *data,
                File_saver& file_saver, Image_format save_format,
                Model_format model_format, int w, int h);
