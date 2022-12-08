#include "vk_mesh.h"

#include <iostream>

#include <tiny_obj_loader.h>

VertexInputDescription Vertex::get_vertex_description() {
  VertexInputDescription description;

  VkVertexInputBindingDescription mainBinding{};
  mainBinding.binding = 0;
  mainBinding.stride = sizeof(Vertex);
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings.push_back(mainBinding);

  VkVertexInputAttributeDescription positionAttribute{};
  positionAttribute.binding = 0;
  positionAttribute.location = 0;
  positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  positionAttribute.offset = offsetof(Vertex, position);

  description.attributes.push_back(positionAttribute);

  VkVertexInputAttributeDescription normalAttribute{};
  normalAttribute.binding = 0;
  normalAttribute.location = 1;
  normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  normalAttribute.offset = offsetof(Vertex, normal);

  description.attributes.push_back(normalAttribute);

  VkVertexInputAttributeDescription colorAttribute{};
  colorAttribute.binding = 0;
  colorAttribute.location = 2;
  colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  colorAttribute.offset = offsetof(Vertex, color);

  description.attributes.push_back(colorAttribute);

  return description;
}

bool Mesh::load_from_obj(const std::string& filename) {
  // Container for the vertex data
  tinyobj::attrib_t attrib;

  // Contains the info for each separate object in the file
  std::vector<tinyobj::shape_t> shapes;

  // Contains the info for each material in the file
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(),
                   nullptr);

  if (!warn.empty()) {
    std::cout << "WARN: " << warn << std::endl;
  }

  if (!err.empty()) {
    std::cerr << err << std::endl;
    return false;
  }

  // Loop over shapes
  for (size_t s = 0; s < shapes.size(); s++) {
    size_t index_offset = 0;
    // Loop over faces(polygon)
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      // Hardcoded loading to triangles
      int fv = 3;

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        Vertex vertex{};

        vertex.position = {
            attrib.vertices[3 * idx.vertex_index + 0],
            attrib.vertices[3 * idx.vertex_index + 1],
            attrib.vertices[3 * idx.vertex_index + 2],
        };

        vertex.normal = {
            attrib.normals[3 * idx.normal_index + 0],
            attrib.normals[3 * idx.normal_index + 1],
            attrib.normals[3 * idx.normal_index + 2],
        };

        vertex.color = vertex.normal;

        _vertices.push_back(vertex);
      }
      index_offset += fv;
    }
  }

  return true;
}