#include "vk_engine.h"
#include "vk_pipeline.h"
#include "vk_types.h"
#include "vk_initializers.h"

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <VkBootstrap.h>

#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <iostream>
#include <fstream>
#include <limits.h>

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

  init_path();

  // Load the core Vulkan structures
  init_vulkan();

  // Create the swapchain
  init_swapchain();

  init_commands();

  init_default_renderpass();

  init_framebuffers();

  init_sync_structures();

  init_pipelines();

  load_meshes();

  // everything went fine
  _isInitialized = true;
}

void VulkanEngine::cleanup() {
  if (_isInitialized) {
    // Make sure the GPU has stopped doing its things
    vkDeviceWaitIdle(_device);

    _mainDeletionQueue.flush();
    vmaDestroyAllocator(_allocator);

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

  // Clear depth at 1
  VkClearValue depthClear;
  depthClear.depthStencil.depth = 1.0f;

  // Start the main renderpass.
  // We will use the clear color from above and the framebuffer of the index the
  // swapchain gave us
  VkRenderPassBeginInfo rpInfo = vkinit::render_pass_begin_info(
      _renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

  // Connect clear values
  rpInfo.clearValueCount = 2;

  VkClearValue clearValues[] = {clearValue, depthClear};

  rpInfo.pClearValues = &clearValues[0];

  vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

  glm::vec3 camPos = {0.0f, 0.0f, -2.0f};

  glm::mat4 view = glm::translate(glm::mat4(1.0f), camPos);
  glm::mat4 projection = glm::perspective(
      glm::radians(70.0f),
      (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 200.0f);
  projection[1][1] *= -1;
  glm::mat4 model = glm::rotate(
      glm::mat4(1.0f), glm::radians(_frameNumber * 0.4f), glm::vec3(0, 1, 0));

  glm::mat4 mesh_matrix = projection * view * model;

  MeshPushConstants constants;
  constants.render_matrix = mesh_matrix;

  vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(MeshPushConstants), &constants);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &_monkeyMesh._vertexBuffer._buffer,
                         &offset);

  vkCmdDraw(cmd, _monkeyMesh._vertices.size(), 1, 0, 0);

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
          if (e.key.keysym.sym == SDLK_SPACE) {
            _selectedShader++;
            if (_selectedShader > 1) _selectedShader = 0;
          }

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

void VulkanEngine::init_path() {
#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
  char buf[MAX_PATH];
  GetModuleFileName(NULL, buf, MAX_PATH);
  path = buf;
  path.erase(path.rfind('\\'));
#elif defined(__APPLE__)
  char buf[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  if (!_NSGetExecutablePath(buf, &bufsize)) {
    path = buf;
    path.erase(path.rfind('/'));
  }
#endif
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

  // Initialize memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _chosenGPU;
  allocatorInfo.device = _device;
  allocatorInfo.instance = _instance;
  vmaCreateAllocator(&allocatorInfo, &_allocator);
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

  _mainDeletionQueue.push_function([=]() {                //
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);  //
  });

  // Depth image size will match the window
  VkExtent3D depthImageExtent = {_windowExtent.width, _windowExtent.height, 1};

  // ! hardcoding the depth format to 32 bit float
  _depthFormat = VK_FORMAT_D32_SFLOAT;

  // the depth image will be an image with the format we selected and Depth
  // Attachment usage flag
  VkImageCreateInfo dimg_info = vkinit::image_create_info(
      _depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      depthImageExtent);

  // for the depth image, we want to allocate it from GPU local memory
  VmaAllocationCreateInfo dimg_allocinfo = {};
  dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  dimg_allocinfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // allocate and create the image
  vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image,
                 &_depthImage._allocation, nullptr);

  // build an image-view for the depth image to use for rendering
  VkImageViewCreateInfo dview_info = vkinit::image_view_create_info(
      _depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

  // add to deletion queues
  _mainDeletionQueue.push_function([=]() {
    vkDestroyImageView(_device, _depthImageView, nullptr);
    vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
  });
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

  _mainDeletionQueue.push_function([=]() {                 //
    vkDestroyCommandPool(_device, _commandPool, nullptr);  //
  });
}

void VulkanEngine::init_default_renderpass() {
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

  VkAttachmentDescription depth_attachment = {};
  depth_attachment.flags = 0;
  depth_attachment.format = _depthFormat;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref = {};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // We are going to use a single subpass
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkSubpassDependency depth_dependency = {};
  depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  depth_dependency.dstSubpass = 0;
  depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  depth_dependency.srcAccessMask = 0;
  depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkSubpassDependency dependencies[2] = {dependency, depth_dependency};

  VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

  // Connect the color attachment to the info
  render_pass_info.attachmentCount = 2;
  render_pass_info.pAttachments = &attachments[0];
  // Connect the subpass to the info
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 2;
  render_pass_info.pDependencies = &dependencies[0];

  VK_CHECK(                                                                  //
      vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass)  //
  );

  _mainDeletionQueue.push_function([=]() {               //
    vkDestroyRenderPass(_device, _renderPass, nullptr);  //
  });
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
    VkImageView attachments[2];
    attachments[0] = _swapchainImageViews[i];
    attachments[1] = _depthImageView;

    fb_info.pAttachments = attachments;
    fb_info.attachmentCount = 2;

    VK_CHECK(
        vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

    _mainDeletionQueue.push_function([=]() {
      vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
      vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    });
  }
}

void VulkanEngine::init_sync_structures() {
  // Create synchronization structures

  VkFenceCreateInfo fenceCreateInfo =
      vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

  VK_CHECK(                                                             //
      vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence)  //
  );

  _mainDeletionQueue.push_function([=]() {           //
    vkDestroyFence(_device, _renderFence, nullptr);  //
  });

  // For the semaphore, we dont need any special flags
  VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

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

  _mainDeletionQueue.push_function([=]() {                    //
    vkDestroySemaphore(_device, _presentSemaphore, nullptr);  //
    vkDestroySemaphore(_device, _renderSemaphore, nullptr);   //
  });
}

bool VulkanEngine::load_shader_module(const std::string filename,
                                      VkShaderModule* outShaderModule) {
  // Open the file with cursor at the end
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "Failed to open file " << filename << std::endl;
    return false;
  }

  // Get the size of the file
  size_t fileSize = (size_t)file.tellg();

  // Allocate a buffer to hold the file data
  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

  // Go back to the beginning of the file
  file.seekg(0);

  // Read the file data into the buffer
  file.read((char*)buffer.data(), fileSize);

  // Close the file
  file.close();

  // Create the shader module
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pNext = nullptr;

  // We need to specify the size of the code in bytes
  createInfo.codeSize = buffer.size() * sizeof(uint32_t);
  createInfo.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    std::cerr << "Failed to create shader module" << std::endl;
    return false;
  }

  *outShaderModule = shaderModule;

  return true;
}

void VulkanEngine::init_pipelines() {
  // Load the shader modules
  VkShaderModule triangleVertexShader;
  VkShaderModule triangleFragmentShader;
  VkShaderModule red_triangleVertexShader;
  VkShaderModule red_triangleFragmentShader;

  if (!load_shader_module(path + "/shaders/triangle.vert.spv",
                          &triangleVertexShader)) {
    std::cerr << "Failed to load vertex shader" << std::endl;
    return;
  } else {
    std::cout << "Vertex shader loaded" << std::endl;
  }

  if (!load_shader_module(path + "/shaders/triangle.frag.spv",
                          &triangleFragmentShader)) {
    std::cerr << "Failed to load fragment shader" << std::endl;
    return;
  } else {
    std::cout << "Fragment shader loaded" << std::endl;
  }

  if (!load_shader_module(path + "/shaders/triangle_red.vert.spv",
                          &red_triangleVertexShader)) {
    std::cerr << "Failed to load vertex shader" << std::endl;
    return;
  } else {
    std::cout << "Vertex shader loaded red" << std::endl;
  }

  if (!load_shader_module(path + "/shaders/triangle_red.frag.spv",
                          &red_triangleFragmentShader)) {
    std::cerr << "Failed to load fragment shader" << std::endl;
    return;
  } else {
    std::cout << "Fragment shader loaded" << std::endl;
  }

  // Build the pipeline layout that controls the iputs/outputs of the shader
  VkPipelineLayoutCreateInfo pipeline_layout_info =
      vkinit::pipeline_layout_create_info();

  VK_CHECK(                                                            //
      vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr,  //
                             &_pipelineLayout)                         //
  );

  // Build the stage-create info for both vertex and fragment shaders
  PipelineBuilder pipelineBuilder;

  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                triangleVertexShader));

  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                triangleFragmentShader));

  // Vertex input controls how to read the vertex data from the vertex buffer
  pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

  // Input assembly controls how to assemble the vertex data
  pipelineBuilder._inputAssembly =
      vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // Build biewport and scissor from the swapchain extents
  pipelineBuilder._viewport.x = 0.0f;
  pipelineBuilder._viewport.y = 0.0f;
  pipelineBuilder._viewport.width = (float)_windowExtent.width;
  pipelineBuilder._viewport.height = (float)_windowExtent.height;
  pipelineBuilder._viewport.minDepth = 0.0f;
  pipelineBuilder._viewport.maxDepth = 1.0f;

  pipelineBuilder._scissor.offset = {0, 0};
  pipelineBuilder._scissor.extent = _windowExtent;

  // Rasterizer takes the geometry that is shaped by the vertices from the
  // vertex shader and turns it into fragments to be colored by the fragment
  // shader; it also performs depth testing, face culling and the scissor test
  pipelineBuilder._rasterizer =
      vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

  // Multisampling is used to perform anti-aliasing
  pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

  // Color blending attachments. Single blend with no blending and writing to
  // RGBA
  pipelineBuilder._colorBlendAttachment =
      vkinit::color_blend_attachment_state();

  // Use triangle layout we created
  pipelineBuilder._pipelineLayout = _pipelineLayout;

  pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(
      true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

  // Build the pipeline
  _trianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

  pipelineBuilder._shaderStages.clear();

  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                red_triangleVertexShader));

  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                red_triangleFragmentShader));

  // Build the pipeline for red triangle
  _redTrianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

  // Build the mesh pipeline
  VertexInputDescription vertexDescription = Vertex::get_vertex_description();

  pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions =
      vertexDescription.attributes.data();
  pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount =
      vertexDescription.attributes.size();

  pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions =
      vertexDescription.bindings.data();
  pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount =
      vertexDescription.bindings.size();

  pipelineBuilder._shaderStages.clear();

  VkShaderModule meshVertShader;
  if (!load_shader_module(path + "/shaders/tri_mesh.vert.spv",
                          &meshVertShader)) {
    std::cout << "Error when building the triangle vertex shader module"
              << std::endl;
  } else {
    std::cout << "Red Triangle vertex shader successfully loaded" << std::endl;
  }

  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                meshVertShader));

  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                triangleFragmentShader));

  VkPipelineLayoutCreateInfo mesh_pipeline_layout_info =
      vkinit::pipeline_layout_create_info();

  VkPushConstantRange push_constant;
  push_constant.offset = 0;
  push_constant.size = sizeof(MeshPushConstants);
  push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
  mesh_pipeline_layout_info.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr,
                                  &_meshPipelineLayout));

  pipelineBuilder._pipelineLayout = _meshPipelineLayout;

  _meshPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

  vkDestroyShaderModule(_device, meshVertShader, nullptr);
  vkDestroyShaderModule(_device, triangleVertexShader, nullptr);
  vkDestroyShaderModule(_device, triangleFragmentShader, nullptr);
  vkDestroyShaderModule(_device, red_triangleVertexShader, nullptr);
  vkDestroyShaderModule(_device, red_triangleFragmentShader, nullptr);

  _mainDeletionQueue.push_function([=]() {
    vkDestroyPipeline(_device, _trianglePipeline, nullptr);
    vkDestroyPipeline(_device, _redTrianglePipeline, nullptr);
    vkDestroyPipeline(_device, _meshPipeline, nullptr);

    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
    vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
  });
}

void VulkanEngine::upload_mesh(Mesh& mesh) {
  // Allocate vertex buffer
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = sizeof(Vertex) * mesh._vertices.size();
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  // Let VMA library know that this sould be writeable bo CPU and readeable by
  // GPU
  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  // Allocate the buffer
  VK_CHECK(                                             //
      vmaCreateBuffer(_allocator,                       //
                      &bufferInfo,                      //
                      &allocInfo,                       //
                      &mesh._vertexBuffer._buffer,      //
                      &mesh._vertexBuffer._allocation,  //
                      nullptr)                          //
  );

  // Add destruction of triangle mesh buffer to the deletion queue
  _mainDeletionQueue.push_function([=]() {
    vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer,
                     mesh._vertexBuffer._allocation);
  });

  // Copy vertex data
  void* data;
  vmaMapMemory(_allocator, mesh._vertexBuffer._allocation, &data);

  memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));

  vmaUnmapMemory(_allocator, mesh._vertexBuffer._allocation);
}

void VulkanEngine::load_meshes() {
  _triangleMesh._vertices.resize(3);

  _triangleMesh._vertices[0].position = {0.75f, 0.75f, 0.0f};
  _triangleMesh._vertices[1].position = {-0.75f, 0.75f, 0.0f};
  _triangleMesh._vertices[2].position = {0.0f, -0.75f, 0.0f};

  _triangleMesh._vertices[0].color = {0.0f, 1.0f, 0.0f};
  _triangleMesh._vertices[1].color = {0.0f, 1.0f, 0.0f};
  _triangleMesh._vertices[2].color = {0.0f, 1.0f, 0.0f};

  _monkeyMesh.load_from_obj(path + "/models/monkey_smooth/monkey_smooth.obj");

  upload_mesh(_triangleMesh);
  upload_mesh(_monkeyMesh);
}
