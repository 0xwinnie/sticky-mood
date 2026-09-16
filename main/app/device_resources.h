#pragma once

#include "ui/resources.h"

// Fills Resources from the blobs embedded in the firmware image by
// target_add_binary_data (see main/CMakeLists.txt). The host simulator loads
// the same blobs from build/font_data and build/art_data instead.
bool load_embedded_resources(Resources &out);
