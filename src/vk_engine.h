#ifndef C12F24BE_7752_44A1_B4B1_AA3E1F0F254D
#define C12F24BE_7752_44A1_B4B1_AA3E1F0F254D

#include "vk_types.h"
#include "vk_deletionQueue.h"
#include "vk_mesh.h"

#include "camera/camera.h"
#include "Utility/FPSCounter.h"

#include <vector>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

constexpr unsigned FRAME_OVERLAP = 2;

struct UploadContext {
  VkFence _uploadFence;
  VkCommandPool _commandPool;
  VkCommandBuffer _commandBuffer;
};

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

struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
};

struct GPUSceneData {
  glm::vec4 fogColor;      // w is for exponent
  glm::vec4 fogDistances;  // x for min, y for max, zw unused
  glm::vec4 ambientColor;
  glm::vec4 lightDirection;  // w for sun power
  glm::vec4 lightColor;
};

struct GPUObjectData {
  glm::mat4 modelMatrix;
};

struct FrameData {
  VkSemaphore _presentSemaphore, _renderSemaphore;
  VkFence _renderFence;

  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;

  AllocatedBuffer cameraBuffer;
  AllocatedBuffer objectBuffer;

  VkDescriptorSet globalDescriptor;
  VkDescriptorSet objectDescriptor;
};

class VulkanEngine {
 public:
  bool _isInitialized{false};
  int _frameNumber{0};

  VkExtent2D _windowExtent{1600, 800};
  int _milisecondsPreviousFrame{0};

  struct SDL_Window* _window{nullptr};

  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
  VkPhysicalDevice _chosenGPU;
  VkPhysicalDeviceProperties _gpuProperties;
  VkDevice _device;

  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

  FrameData _frames[FRAME_OVERLAP];

  VkRenderPass _renderPass;

  VkSurfaceKHR _surface;
  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkFramebuffer> _framebuffers;
  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;

  DeletionQueue _mainDeletionQueue;

  VmaAllocator _allocator;

  VkImageView _depthImageView;
  AllocatedImage _depthImage;
  VkFormat _depthFormat;

  VkDescriptorSetLayout _globalSetLayout;
  VkDescriptorSetLayout _objectSetLayout;
  VkDescriptorPool _descriptorPool;

  GPUSceneData _sceneParameters;
  AllocatedBuffer _sceneParameterBuffer;

  UploadContext _uploadContext;

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

  FrameData& get_current_frame(void);

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);

  size_t pad_uniform_buffer_size(size_t originalSize);

  void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

 private:
  std::string path;

  bool bQuit = false;

  int _mouseX{0}, _mouseY{0};

  void init_path(void);

  void init_vulkan(void);

  void init_swapchain(void);

  void init_default_renderpass(void);

  void init_framebuffers(void);

  void init_commands(void);

  void init_sync_structures(void);

  void update(void);

  void handle_input(void);

  bool load_shader_module(const std::string filePath,
                          VkShaderModule* outShaderModule);

  void init_pipelines(void);

  void load_meshes(void);

  void upload_mesh(Mesh& mesh);

  void init_scene(void);

  void init_descriptors(void);
};

#endif /* C12F24BE_7752_44A1_B4B1_AA3E1F0F254D */
