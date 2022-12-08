#ifndef C12F24BE_7752_44A1_B4B1_AA3E1F0F254D
#define C12F24BE_7752_44A1_B4B1_AA3E1F0F254D

#include "vk_types.h"
#include "vk_deletionQueue.h"
#include "vk_mesh.h"

#include <vector>
#include <string>

#include <glm/glm.hpp>

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 render_matrix;
};

class VulkanEngine {
 public:
  int _selectedShader{0};

  bool _isInitialized{false};
  int _frameNumber{0};

  VkExtent2D _windowExtent{1600, 800};

  struct SDL_Window* _window{nullptr};

  VmaAllocator _allocator;  // Vulkan Memory Allocator

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

  VkImageView _depthImageView;
  AllocatedImage _depthImage;
  VkFormat _depthFormat;

  VkSemaphore _presentSemaphore, _renderSemaphore;
  VkFence _renderFence;

  VkPipelineLayout _pipelineLayout;
  VkPipelineLayout _meshPipelineLayout;

  VkPipeline _trianglePipeline;
  VkPipeline _redTrianglePipeline;

  // Image format expected by the windowing system
  VkFormat _swapchainImageFormat;

  // Array of images from the swapchain
  std::vector<VkImage> _swapchainImages;

  std::vector<VkFramebuffer> _framebuffers;

  // Array of image-views from the swapchain
  std::vector<VkImageView> _swapchainImageViews;

  VkPipeline _meshPipeline;
  Mesh _triangleMesh;
  Mesh _monkeyMesh;

  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

 private:
  std::string path;

  void init_path();

  void init_vulkan();

  void init_swapchain();

  void init_default_renderpass();

  void init_framebuffers();

  void init_commands();

  void init_sync_structures();

  bool load_shader_module(const std::string filePath,
                          VkShaderModule* outShaderModule);

  void init_pipelines();

  void load_meshes();

  void upload_mesh(Mesh& mesh);
};

#endif /* C12F24BE_7752_44A1_B4B1_AA3E1F0F254D */
