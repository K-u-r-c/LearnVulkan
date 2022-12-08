#ifndef DDE59128_6F28_496A_83FC_0804E78EE5E5
#define DDE59128_6F28_496A_83FC_0804E78EE5E5

#include "vk_types.h"

#include <vector>

class PipelineBuilder {
 public:
  std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
  VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
  VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
  VkViewport _viewport;
  VkRect2D _scissor;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineColorBlendAttachmentState _colorBlendAttachment;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineLayout _pipelineLayout;
  VkPipelineDepthStencilStateCreateInfo _depthStencil;

  VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};

#endif /* DDE59128_6F28_496A_83FC_0804E78EE5E5 */
