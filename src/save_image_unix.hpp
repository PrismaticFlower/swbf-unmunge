#pragma once

#include "app_options.hpp"
#include "file_saver.hpp"

#include "opencv2/opencv.hpp"

void save_image(std::string_view name, cv::Mat image,
                File_saver& file_saver, Image_format save_format,
                Model_format model_format);
