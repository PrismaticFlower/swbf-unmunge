#pragma once

#include <filesystem>
#include <string_view>

#include <DirectXTex.h>

class File_saver;

void save_image_tga(const std::filesystem::path& save_path, DirectX::Image image);