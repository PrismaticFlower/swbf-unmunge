#pragma once

#include "app_options.hpp"
#include "file_saver.hpp"

#include "DirectXTex.h"

void save_image(std::string_view name, DirectX::ScratchImage image,
                File_saver& file_saver, Image_format save_format);
