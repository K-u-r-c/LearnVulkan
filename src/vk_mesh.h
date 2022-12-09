#ifndef B75A0784_44B4_4B00_A0C0_899C54E55663
#define B75A0784_44B4_4B00_A0C0_899C54E55663

#include "vk_types.h"
#include <vector>
#include <string>
#include <glm/vec3.hpp>

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags{0};
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;

  static VertexInputDescription get_vertex_description();
};

struct Mesh {
  std::vector<Vertex> _vertices;

  AllocatedBuffer _vertexBuffer;

  bool load_from_obj(const std::string& filename);
};

#endif /* B75A0784_44B4_4B00_A0C0_899C54E55663 */
