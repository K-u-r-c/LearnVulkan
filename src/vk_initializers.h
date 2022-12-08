﻿#ifndef B277EA88_4840_4CAD_AE3C_C90EDFE4016A
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

VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(
    VkShaderStageFlagBits stage, VkShaderModule shaderModule);

VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(
    VkPrimitiveTopology topology);

VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(
    VkPolygonMode polygonMode);

VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

VkPipelineColorBlendAttachmentState color_blend_attachment_state();

VkPipelineColorBlendAttachmentState color_blend_attachment_state();

VkPipelineLayoutCreateInfo pipeline_layout_create_info();

VkImageCreateInfo image_create_info(VkFormat format,
                                    VkImageUsageFlags usageFlags,
                                    VkExtent3D extent);

VkImageViewCreateInfo image_view_create_info(VkFormat format, VkImage image,
                                             VkImageAspectFlags aspectFlags);

VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(
    bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

}  // namespace vkinit

#endif /* B277EA88_4840_4CAD_AE3C_C90EDFE4016A */
