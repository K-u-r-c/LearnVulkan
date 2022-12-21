#ifndef C99840F7_0AAB_4255_A5DB_A828BA8B5708
#define C99840F7_0AAB_4255_A5DB_A828BA8B5708

#include "vk_types.h"
#include "vk_engine.h"

#include <string>

namespace vkutil {
bool load_image_from_file(VulkanEngine& engine, const std::string file,
                          AllocatedImage& outImage);
}

#endif /* C99840F7_0AAB_4255_A5DB_A828BA8B5708 */
