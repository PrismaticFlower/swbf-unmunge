#pragma once

#include "app_options.hpp"
#include "model_scene.hpp"

class File_saver;

namespace model::gltf {

void save_scene(scene::Scene scene, File_saver& file_saver);

}