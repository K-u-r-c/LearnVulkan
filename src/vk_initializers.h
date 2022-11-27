#ifndef B277EA88_4840_4CAD_AE3C_C90EDFE4016A
#define B277EA88_4840_4CAD_AE3C_C90EDFE4016A

#include "vk_types.h"

namespace vkinit {

VkCommandPoolCreateInfo command_pool_create_info(
    uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

VkCommandBufferAllocateInfo command_buffer_allocate_info(
    VkCommandPool pool, uint32_t count = 1,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkCommandBufferBeginInfo command_buffer_begin_info(
    VkCommandBufferUsageFlags flags = 0);

VkFramebufferCreateInfo framebuffer_create_info(VkRenderPass renderPass,
                                                VkExtent2D extent);

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo submit_info(VkCommandBuffer* cmd);

VkPresentInfoKHR present_info();

VkRenderPassBeginInfo render_pass_begin_info(VkRenderPass renderPass,
                                             VkExtent2D extent,
                                             VkFramebuffer framebuffer);

}  // namespace vkinit

#endif /* B277EA88_4840_4CAD_AE3C_C90EDFE4016A */
