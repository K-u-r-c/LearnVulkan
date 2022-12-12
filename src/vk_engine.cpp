﻿#include "vk_engine.h"
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

Material* VulkanEngine::create_material(VkPipeline pipeline,
                                        VkPipelineLayout layout,
                                        const std::string& name) {
  Material mat;
  mat.pipeline = pipeline;
  mat.pipelineLayout = layout;
  _materials[name] = mat;
  return &_materials[name];
}

Material* VulkanEngine::get_material(const std::string& name) {
  auto it = _materials.find(name);
  if (it == _materials.end()) {
    return nullptr;
  } else {
    return &(*it).second;
  }
}

Mesh* VulkanEngine::get_mesh(const std::string& name) {
  auto it = _meshes.find(name);
  if (it == _meshes.end()) {
    return nullptr;
  } else {
    return &(*it).second;
  }
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first,
                                int count) {
  // make a model view matrix for rendering the object camera view
  glm::mat4 view = glm::translate(glm::mat4(1.f), _cameraXYZPos);

  view = glm::rotate(view, glm::radians((float)_mouseRelY * _mouseSensitivity),
                     glm::vec3(1.f, 0.f, 0.f));
  view = glm::rotate(view, glm::radians((float)_mouseRelX * _mouseSensitivity),
                     glm::vec3(0.f, 1.f, 0.f));

  // camera projection
  glm::mat4 projection =
      glm::perspective(glm::radians(70.f),
                       static_cast<float>(_windowExtent.width) /
                           static_cast<float>(_windowExtent.height),
                       0.1f, 200.0f);
  projection[1][1] *= -1;

  Mesh* lastMesh = nullptr;
  Material* lastMaterial = nullptr;
  for (int i = 0; i < count; i++) {
    RenderObject& object = first[i];

    // only bind the pipeline if it doesn't match with the already bound one
    if (object.material != lastMaterial) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        object.material->pipeline);
      lastMaterial = object.material;
    }

    glm::mat4 model = object.transformMatrix;
    // final render matrix, that we are calculating on the cpu
    glm::mat4 mesh_matrix = projection * view * model;

    MeshPushConstants constants;
    constants.render_matrix = mesh_matrix;

    // upload the mesh to the GPU via push constants
    vkCmdPushConstants(cmd, object.material->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &constants);

    // only bind the mesh if it's a different one from last bind
    if (object.mesh != lastMesh) {
      // bind the mesh vertex buffer with offset 0
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer,
                             &offset);
      lastMesh = object.mesh;
    }
    // we can now draw
    vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, 0);
  }
}

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

  init_scene();

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
  VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence,
                           VK_TRUE, 1000000000));
  VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

  VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

  // Get the index of the next available swapchain image:
  uint32_t swapchainImageIndex;
  VK_CHECK(                                                         //
      vkAcquireNextImageKHR(_device,                                //
                            _swapchain,                             //
                            1000000000,                             //
                            get_current_frame()._presentSemaphore,  //
                            nullptr,                                //
                            &swapchainImageIndex)                   //
  );

  VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  VkClearValue clearValue;
  clearValue.color = {{0.01f, 0.01f, 0.01f, 1.0f}};

  // Clear depth at 1
  VkClearValue depthClear;
  depthClear.depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo rpInfo = vkinit::render_pass_begin_info(
      _renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

  // Connect clear values
  rpInfo.clearValueCount = 2;

  VkClearValue clearValues[] = {clearValue, depthClear};

  rpInfo.pClearValues = &clearValues[0];

  vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  draw_objects(cmd, _renderables.data(), _renderables.size());

  // End the renderpass
  vkCmdEndRenderPass(cmd);
  // End the command buffer recording
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo submit = vkinit::submit_info(&cmd);
  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.pWaitDstStageMask = &waitStage;

  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &get_current_frame()._presentSemaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &get_current_frame()._renderSemaphore;

  VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit,
                         get_current_frame()._renderFence));

  VkPresentInfoKHR presentInfo = vkinit::present_info();

  presentInfo.pSwapchains = &_swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

  _frameNumber++;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool bQuit = false;

  bool mouseRelative = false;
  SDL_SetRelativeMouseMode(SDL_FALSE);

  while (!bQuit) {
    // SDL event handling
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) bQuit = true;

      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) mouseRelative = false;
        SDL_SetRelativeMouseMode(mouseRelative ? SDL_TRUE : SDL_FALSE);
      }

      if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
          mouseRelative = true;
          SDL_SetRelativeMouseMode(mouseRelative ? SDL_TRUE : SDL_FALSE);
        }
      }

      if (e.type == SDL_MOUSEWHEEL)
        _cameraXYZPos.z <= 0.0f ? _cameraXYZPos.z += e.wheel.y* 0.1f
                                : _cameraXYZPos.z = 0.0f;

      if (e.type == SDL_MOUSEMOTION && mouseRelative) {
        _mouseRelX += e.motion.xrel;
        _mouseRelY += e.motion.yrel;
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
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
      _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (int i = 0; i < FRAME_OVERLAP; i++) {
    VK_CHECK(                                          //
        vkCreateCommandPool(_device,                   //
                            &commandPoolInfo,          //
                            nullptr,                   //
                            &_frames[i]._commandPool)  //
    );

    // Allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo =
        vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

    VK_CHECK(                                                     //
        vkAllocateCommandBuffers(_device,                         //
                                 &cmdAllocInfo,                   //
                                 &_frames[i]._mainCommandBuffer)  //
    );

    _mainDeletionQueue.push_function([=]() {                            //
      vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);  //
    });
  }
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
  VkFenceCreateInfo fenceCreateInfo =
      vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

  VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

  for (int i = 0; i < FRAME_OVERLAP; i++) {
    VK_CHECK(                                    //
        vkCreateFence(_device,                   //
                      &fenceCreateInfo,          //
                      nullptr,                   //
                      &_frames[i]._renderFence)  //
    );

    _mainDeletionQueue.push_function([=]() {                      //
      vkDestroyFence(_device, _frames[i]._renderFence, nullptr);  //
    });

    VK_CHECK(                                             //
        vkCreateSemaphore(_device,                        //
                          &semaphoreCreateInfo,           //
                          nullptr,                        //
                          &_frames[i]._presentSemaphore)  //
    );
    VK_CHECK(                                            //
        vkCreateSemaphore(_device,                       //
                          &semaphoreCreateInfo,          //
                          nullptr,                       //
                          &_frames[i]._renderSemaphore)  //
    );

    _mainDeletionQueue.push_function([=]() {                               //
      vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);  //
      vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);   //
    });
  }
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
  VkShaderModule vertexShader;
  VkShaderModule fragmentShader;

  if (!load_shader_module(path + "/shaders/triangle.vert.spv", &vertexShader)) {
    std::cerr << "Failed to load vertex shader" << std::endl;
    return;
  } else {
    std::cout << "Vertex shader loaded" << std::endl;
  }

  if (!load_shader_module(path + "/shaders/triangle.frag.spv",
                          &fragmentShader)) {
    std::cerr << "Failed to load fragment shader" << std::endl;
    return;
  } else {
    std::cout << "Fragment shader loaded" << std::endl;
  }

  VkPipelineLayoutCreateInfo mesh_pipeline_layout_info =
      vkinit::pipeline_layout_create_info();

  VkPushConstantRange push_constant;
  push_constant.offset = 0;
  push_constant.size = sizeof(MeshPushConstants);
  push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
  mesh_pipeline_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout mesh_pipeline_layout;

  VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr,
                                  &mesh_pipeline_layout));

  PipelineBuilder pipelineBuilder;
  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                vertexShader));
  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                fragmentShader));

  pipelineBuilder._pipelineLayout = mesh_pipeline_layout;
  pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
  pipelineBuilder._inputAssembly =
      vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._viewport.x = 0.0f;
  pipelineBuilder._viewport.y = 0.0f;
  pipelineBuilder._viewport.width = (float)_windowExtent.width;
  pipelineBuilder._viewport.height = (float)_windowExtent.height;
  pipelineBuilder._viewport.minDepth = 0.0f;
  pipelineBuilder._viewport.maxDepth = 1.0f;

  pipelineBuilder._scissor.offset = {0, 0};
  pipelineBuilder._scissor.extent = _windowExtent;

  pipelineBuilder._rasterizer =
      vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

  pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

  pipelineBuilder._colorBlendAttachment =
      vkinit::color_blend_attachment_state();

  pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(
      true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

  VertexInputDescription vertexDescription = Vertex::get_vertex_description();

  pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions =
      vertexDescription.attributes.data();
  pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertexDescription.attributes.size());

  pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions =
      vertexDescription.bindings.data();
  pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount =
      static_cast<uint32_t>(vertexDescription.bindings.size());

  VkPipeline meshPipeline =
      pipelineBuilder.build_pipeline(_device, _renderPass);

  create_material(meshPipeline, mesh_pipeline_layout, "defaultmesh");

  vkDestroyShaderModule(_device, vertexShader, nullptr);
  vkDestroyShaderModule(_device, fragmentShader, nullptr);

  _mainDeletionQueue.push_function([=]() {
    vkDestroyPipeline(_device, meshPipeline, nullptr);
    vkDestroyPipelineLayout(_device, mesh_pipeline_layout, nullptr);
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
  Mesh triangleMesh;

  triangleMesh._vertices.resize(3);

  triangleMesh._vertices[0].position = {0.75f, 0.75f, 0.0f};
  triangleMesh._vertices[1].position = {-0.75f, 0.75f, 0.0f};
  triangleMesh._vertices[2].position = {0.0f, -0.75f, 0.0f};

  triangleMesh._vertices[0].color = {0.0f, 1.0f, 0.0f};
  triangleMesh._vertices[1].color = {0.0f, 1.0f, 0.0f};
  triangleMesh._vertices[2].color = {0.0f, 1.0f, 0.0f};

  Mesh monkeyMesh;
  monkeyMesh.load_from_obj(path + "/models/monkey_smooth/monkey_smooth.obj");

  Mesh myShapeMesh;
  myShapeMesh.load_from_obj(path + "/models/my_shape/my_shape.obj");

  upload_mesh(triangleMesh);
  upload_mesh(monkeyMesh);
  upload_mesh(myShapeMesh);

  _meshes["monkey"] = monkeyMesh;
  _meshes["triangle"] = triangleMesh;
  _meshes["myshape"] = myShapeMesh;
}

void VulkanEngine::init_scene() {
  RenderObject monkey;
  monkey.mesh = get_mesh("triangle");
  monkey.material = get_material("defaultmesh");
  monkey.transformMatrix = glm::mat4{1.0f};

  _renderables.push_back(monkey);

  for (int x = -20; x <= 20; x++) {
    for (int y = -20; y <= 20; y++) {
      RenderObject tri;
      tri.mesh = get_mesh("monkey");
      tri.material = get_material("defaultmesh");
      glm::mat4 translation =
          glm::translate(glm::mat4{1.0}, glm::vec3(x, 0, y));
      glm::mat4 scale = glm::scale(glm::mat4{1.0}, glm::vec3(0.2, 0.2, 0.2));
      tri.transformMatrix = translation * scale;

      _renderables.push_back(tri);
    }
  }

  RenderObject myShape;
  myShape.mesh = get_mesh("myshape");
  myShape.material = get_material("defaultmesh");

  glm::mat4 translation = glm::translate(glm::mat4{1.0}, glm::vec3(0, 5, 0));
  glm::mat4 scale = glm::scale(glm::mat4{1.0}, glm::vec3(0.2, 0.2, 0.2));

  myShape.transformMatrix = translation * scale;

  _renderables.push_back(myShape);
}

FrameData& VulkanEngine::get_current_frame() {
  return _frames[_frameNumber % FRAME_OVERLAP];
}