#pragma once
#include <vector>
int decodePNG(unsigned char *&out_image, int &image_width, int &image_height, const unsigned char *in_png, size_t in_size, bool convert_to_rgba32 = true);
