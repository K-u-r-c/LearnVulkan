#ifndef C12F24BE_7752_44A1_B4B1_AA3E1F0F254D
#define C12F24BE_7752_44A1_B4B1_AA3E1F0F254D

#include "vk_types.h"
#include "vk_deletionQueue.h"
#include "vk_mesh.h"

#include <vector>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 render_matrix;
};

struct Material {
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
};

struct RenderObject {
  Mesh* mesh;
  Material* material;
  glm::mat4 transformMatrix;
};

class VulkanEngine {
 public:
  bool _isInitialized{false};
  int _frameNumber{0};

  VkExtent2D _windowExtent{1600, 800};

  glm::vec3 _cameraXYZPos = glm::vec3(0.0f);
  int _mouseRelX = 0;
  int _mouseRelY = 0;
  float _mouseSensitivity = 0.1f;

  struct SDL_Window* _window{nullptr};

  VkInstance _instance;                       // Vulkan library handle
  VkDebugUtilsMessengerEXT _debug_messenger;  // Vulkan debug output handle
  VkPhysicalDevice _chosenGPU;                // Vulkan physical device
  VkDevice _device;                           // Vulkan device for commands

  VkSemaphore _presentSemaphore, _renderSemaphore;
  VkFence _renderFence;

  VkQueue _graphicsQueue;         // Queue we will submit to
  uint32_t _graphicsQueueFamily;  // Family of that queue

  VkCommandPool _commandPool;          // The command pool for commands
  VkCommandBuffer _mainCommandBuffer;  // The buffer we will rocerd into

  VkRenderPass _renderPass;

  VkSurfaceKHR _surface;
  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkFramebuffer> _framebuffers;
  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;

  DeletionQueue _mainDeletionQueue;

  VmaAllocator _allocator;  // Vulkan Memory Allocator

  VkImageView _depthImageView;
  AllocatedImage _depthImage;
  VkFormat _depthFormat;

  // initializes everything in the engine
  void init(void);

  // shuts down the engine
  void cleanup(void);

  // draw loop
  void draw(void);

  // run main loop
  void run(void);

  std::vector<RenderObject> _renderables;

  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;

  Material* create_material(VkPipeline pipeline, VkPipelineLayout layout,
                            const std::string& name);

  Material* get_material(const std::string& name);

  Mesh* get_mesh(const std::string& name);

  void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);

 private:
  std::string path;

  void init_path(void);

  void init_vulkan(void);

  void init_swapchain(void);

  void init_default_renderpass(void);

  void init_framebuffers(void);

  void init_commands(void);

  void init_sync_structures(void);

  bool load_shader_module(const std::string filePath,
                          VkShaderModule* outShaderModule);

  void init_pipelines(void);

  void load_meshes(void);

  void upload_mesh(Mesh& mesh);

  void init_scene(void);
};

#endif /* C12F24BE_7752_44A1_B4B1_AA3E1F0F254D */
