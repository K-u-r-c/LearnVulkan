#include "vk_engine.h"

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

#include "vk_types.h"
#include "vk_initializers.h"

#include <VkBootstrap.h>

#include <iostream>

// Defined to imediately abort when there is an arror.
#define VK_CHECK(x)                                               \
  do {                                                            \
    VkResult err = x;                                             \
    if (err) {                                                    \
      std::cout << "Detected Vulkan error: " << err << std::endl; \
      abort();                                                    \
    }                                                             \
  } while (0);

void VulkanEngine::init() {
  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  _window = SDL_CreateWindow(   // SDL window creation
      "Vulkan Engine",          // Window title
      SDL_WINDOWPOS_UNDEFINED,  // Window pos x
      SDL_WINDOWPOS_UNDEFINED,  // Window pos y
      _windowExtent.width,      // Window width in pixels
      _windowExtent.height,     // Window heifht in pixels
      window_flags              // Window flags
  );

  // Load the core Vulkan structures
  init_vulkan();

  // Create the swapchain
  init_swapchain();

  init_commands();

  init_default_renderpass();

  init_framebuffers();

  init_sync_structures();

  // everything went fine
  _isInitialized = true;
}

void VulkanEngine::cleanup() {
  if (_isInitialized) {
    // Make sure the GPU has stopped doing its things
    vkDeviceWaitIdle(_device);

    vkDestroyCommandPool(_device, _commandPool, nullptr);

    // Destroy sync objects
    vkDestroyFence(_device, _renderFence, nullptr);
    vkDestroySemaphore(_device, _presentSemaphore, nullptr);
    vkDestroySemaphore(_device, _renderSemaphore, nullptr);

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    // Destroy the main renderpass
    vkDestroyRenderPass(_device, _renderPass, nullptr);

    // Destroy the framebuffers and their attachments
    for (int i = 0; i < _framebuffers.size(); i++) {
      vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
      vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }

    vkDestroySurfaceKHR(_instance, _surface, nullptr);

    vkDestroyDevice(_device, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroyInstance(_instance, nullptr);

    SDL_DestroyWindow(_window);
  }
}

void VulkanEngine::draw() {
  // If window is minimized skip drawing
  if (SDL_GetWindowFlags(_window) & SDL_WINDOW_MINIMIZED) {
    return;
  }

  // Wait untill the GPU has finished rendering the last frame. Timeout of 1 sec
  VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, VK_TRUE, 1000000000));
  VK_CHECK(vkResetFences(_device, 1, &_renderFence));

  // Now that we are sure that the commands are finished executing, we can
  // safely reset the command buffer to begin recording again
  VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

  // Get the index of the next available swapchain image:
  uint32_t swapchainImageIndex;
  VK_CHECK(                                        //
      vkAcquireNextImageKHR(_device,               //
                            _swapchain,            //
                            1000000000,            //
                            _presentSemaphore,     //
                            nullptr,               //
                            &swapchainImageIndex)  //
  );

  VkCommandBuffer cmd = _mainCommandBuffer;

  // Begin the command buffer recording. We will use this command buffer exactly
  // once so we let Vulkan know about it.
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // Make a clear-color from frame number. This will flash with a 120*pi frame
  // period
  VkClearValue clearValue;
  float flash = abs(sin(_frameNumber / 120.0f));
  clearValue.color = {{0.0f, 0.0f, flash, 1.0f}};

  // Start the main renderpass.
  // We will use the clear color from above and the framebuffer of the index the
  // swapchain gave us
  VkRenderPassBeginInfo rpInfo = vkinit::render_pass_begin_info(
      _renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

  // Connect clear values
  rpInfo.clearValueCount = 1;
  rpInfo.pClearValues = &clearValue;

  vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  // End the renderpass
  vkCmdEndRenderPass(cmd);
  // End the command buffer recording
  VK_CHECK(vkEndCommandBuffer(cmd));

  // prepare the submission to the queue.
  // we want to wait on the _presentSemaphore, as that semaphore is signaled
  // when the swapchain is ready we will signal the _renderSemaphore, to signal
  // that rendering has finished

  VkSubmitInfo submit = vkinit::submit_info(&cmd);
  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.pWaitDstStageMask = &waitStage;

  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &_presentSemaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &_renderSemaphore;

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));

  // this will put the image we just rendered into the visible window.
  // we want to wait on the _renderSemaphore for that,
  // as it's necessary that drawing commands have finished before the image is
  // displayed to the user
  VkPresentInfoKHR presentInfo = vkinit::present_info();

  presentInfo.pSwapchains = &_swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &_renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

  // Increase the number of frames drawn
  _frameNumber++;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool bQuit = false;

  // main loop
  while (!bQuit) {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      // close the window when user alt-f4s or clicks the X button
      if (e.type == SDL_QUIT) bQuit = true;

      // Handle keyboard events
      switch (e.type) {
        case SDL_KEYDOWN:
          std::cout << "Key pressed | Event key: "
                    << SDL_GetKeyName(e.key.keysym.sym) << std::endl;
          break;

        case SDL_KEYUP:
          std::cout << "Key released | Event key: "
                    << SDL_GetKeyName(e.key.keysym.sym) << std::endl;
          break;

        default:
          break;
      }
    }

    draw();
  }
}

void VulkanEngine::init_vulkan() {
  vkb::InstanceBuilder builder;

  // Creates the Vulkan instance with basic debug features
  auto inst_ret = builder.set_app_name("Vulkan Application")
                      .request_validation_layers(true)
                      .require_api_version(1, 1, 0)
                      .use_default_debug_messenger()
#ifdef __APPLE__
                      .enable_extension("VK_MVK_macos_surface")
#endif
                      .build();

  if (!inst_ret) {
    std::cerr << "Failed to create Vulkan instance. Error: "
              << inst_ret.error().message() << std::endl;
  }

  vkb::Instance vkb_inst = inst_ret.value();

  // Store the instance
  _instance = vkb_inst.instance;
  // Store the debug messenger
  _debug_messenger = vkb_inst.debug_messenger;

  // Get the surface of the window we opened with SDL
  if (SDL_FALSE == SDL_Vulkan_CreateSurface(_window, _instance, &_surface)) {
    std::cerr << "Failed to create surface, SDL Error: " << SDL_GetError()
              << std::endl;
  }

  // Use vkbootstrap to select a GPU
  // We want a GPU that can write to the SDL surface and supports Vulkan 1.1
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  auto phys_ret = selector                        //
                      .set_minimum_version(1, 1)  //
                      .set_surface(_surface)      //
                      .select();                  //

  if (!phys_ret) std::cout << phys_ret.error().message() << std::endl;

  // Create the final Vulkan device
  vkb::DeviceBuilder deviceBuilder{phys_ret.value()};

  auto dev_ret = deviceBuilder.build();
  if (!dev_ret) {
    std::cerr << "Failed to create Vulkan device. Error: "
              << dev_ret.error().message() << std::endl;
  }

  // Get the VkDevice handle used in the rest of a Vulkan application
  _device = dev_ret.value().device;
  _chosenGPU = phys_ret.value().physical_device;

  // Get graphics queue using vkbootstrap
  _graphicsQueue = dev_ret.value().get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamily =
      dev_ret.value().get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain() {
  vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

  vkb::Swapchain vkbSwapchain =
      swapchainBuilder
          .use_default_format_selection()                                 //
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)             //
          .set_desired_extent(_windowExtent.width, _windowExtent.height)  //
          .build()                                                        //
          .value();                                                       //

  // Store swapchain and its related images
  _swapchain = vkbSwapchain.swapchain;
  _swapchainImages = vkbSwapchain.get_images().value();
  _swapchainImageViews = vkbSwapchain.get_image_views().value();

  _swapchainImageFormat = vkbSwapchain.image_format;
}

void VulkanEngine::init_commands() {
  // Create a command pool for commands submitted to the graphics queue
  // we also want the pool to allow for resetting of individual command buffers
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
      _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  VK_CHECK(                                                                   //
      vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool)  //
  );

  // Allocate the default command buffer that we will use for rendering
  VkCommandBufferAllocateInfo cmdAllocInfo =
      vkinit::command_buffer_allocate_info(_commandPool, 1);

  VK_CHECK(                                                                  //
      vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer)  //
  );
}

void VulkanEngine::init_default_renderpass() {
  // we define an attachment description for our main color image
  // the attachment is loaded as "clear" when renderpass start
  // the attachment is stored when renderpass ends
  // the attachment layout starts as "undefined", and transitions to "Present"
  // so its possible to display it we dont care about stencil, and dont use
  // multisampling

  // The renderpass will use this color attachment.
  VkAttachmentDescription color_attachment = {};

  // The attachment will have the format needed by the swapchain
  color_attachment.format = _swapchainImageFormat;

  // 1 sample, we won't be doing any MSAA
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

  // We clear when this attachment is loaded
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

  // We keep the attachment stored when the renderpass finishes
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  // We don't care about stencil
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  // We dob't know or care about the initial layout of the attachment
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // We want the attachment to be ready for presentation at the end of the
  // renderpass
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {};

  // Attachment number will index into the pAttachments array in the parent
  // renderpass VkRenderPassCreateInfo
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // We are going to use a single subpass
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

  // Connect the color attachment to the info
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  // Connect the subpass to the info
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;

  VK_CHECK(                                                                  //
      vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass)  //
  );
}

void VulkanEngine::init_framebuffers() {
  // Create the framebuffers for the swapchain image. Thiss will connect the
  // preder-pass ti the images for rendering
  VkFramebufferCreateInfo fb_info = {};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext = nullptr;

  fb_info.renderPass = _renderPass;
  fb_info.attachmentCount = 1;
  fb_info.width = _windowExtent.width;
  fb_info.height = _windowExtent.height;
  fb_info.layers = 1;

  // Grab how many images we have in the swapchain
  const uint32_t swapchain_imagecount =
      static_cast<uint32_t>(_swapchainImages.size());
  _framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

  // Create a framebuffer for each image view in the swapchain
  for (uint32_t i = 0; i < swapchain_imagecount; i++) {
    fb_info.pAttachments = &_swapchainImageViews[i];

    VK_CHECK(                                                               //
        vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i])  //
    );
  }
}

void VulkanEngine::init_sync_structures() {
  // Create synchronization structures

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.pNext = nullptr;

  // We want to create the fence with the Create Signaled flag, so we can wait
  // on it before using it on a GPU command (for the first time)
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(                                                             //
      vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence)  //
  );

  // For the semaphore, we dont need any special flags
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreCreateInfo.pNext = nullptr;
  semaphoreCreateInfo.flags = 0;

  VK_CHECK(                                    //
      vkCreateSemaphore(_device,               //
                        &semaphoreCreateInfo,  //
                        nullptr,               //
                        &_presentSemaphore)    //
  );
  VK_CHECK(                                    //
      vkCreateSemaphore(_device,               //
                        &semaphoreCreateInfo,  //
                        nullptr,               //
                        &_renderSemaphore)     //
  );
}