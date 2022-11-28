#ifndef C12F24BE_7752_44A1_B4B1_AA3E1F0F254D
#define C12F24BE_7752_44A1_B4B1_AA3E1F0F254D

#include "vk_types.h"
#include "vk_deletionQueue.h"

#include <vector>

class VulkanEngine {
 public:
  int _selectedShader{0};

  bool _isInitialized{false};
  int _frameNumber{0};

  VkExtent2D _windowExtent{1600, 800};

  struct SDL_Window* _window{nullptr};

  DeletionQueue _mainDeletionQueue;

  VkInstance _instance;                       // Vulkan library handle
  VkDebugUtilsMessengerEXT _debug_messenger;  // Vulkan debug output handle
  VkPhysicalDevice _chosenGPU;                // Vulkan physical device
  VkDevice _device;                           // Vulkan device for commands
  VkSurfaceKHR _surface;                      // Vulkan window surface

  VkSwapchainKHR _swapchain;

  VkQueue _graphicsQueue;         // Queue we will submit to
  uint32_t _graphicsQueueFamily;  // Family of that queue

  VkCommandPool _commandPool;          // The command pool for commands
  VkCommandBuffer _mainCommandBuffer;  // The buffer we will rocerd into

  VkRenderPass _renderPass;

  VkSemaphore _presentSemaphore, _renderSemaphore;
  VkFence _renderFence;

  VkPipelineLayout _pipelineLayout;

  VkPipeline _trianglePipeline;
  VkPipeline _redTrianglePipeline;

  // Image format expected by the windowing system
  VkFormat _swapchainImageFormat;

  // Array of images from the swapchain
  std::vector<VkImage> _swapchainImages;

  std::vector<VkFramebuffer> _framebuffers;

  // Array of image-views from the swapchain
  std::vector<VkImageView> _swapchainImageViews;

  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

 private:
  void init_vulkan();

  void init_swapchain();

  void init_default_renderpass();

  void init_framebuffers();

  void init_commands();

  void init_sync_structures();

  bool load_shader_module(const char* filePath,
                          VkShaderModule* outShaderModule);

  void init_pipelines();
};

#endif /* C12F24BE_7752_44A1_B4B1_AA3E1F0F254D */
