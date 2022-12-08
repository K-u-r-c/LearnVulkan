#ifndef CF0A5BD0_100E_4F0A_A9C3_EB591C123176
#define CF0A5BD0_100E_4F0A_A9C3_EB591C123176

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

struct AllocatedBuffer {
  VkBuffer _buffer;
  VmaAllocation _allocation;
};

struct AllocatedImage {
  VkImage _image;
  VmaAllocation _allocation;
};

#endif /* CF0A5BD0_100E_4F0A_A9C3_EB591C123176 */
