﻿# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("vulkan_guide")

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin")

if(XCODE)
  file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/vulkan_guide.app/Contents/Resources/shaders")
  file(COPY ${CMAKE_CURRENT_LIST_DIR}/assets/models DESTINATION "${PROJECT_SOURCE_DIR}/bin/vulkan_guide.app/Contents/Resources/")
else()
  file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/shaders")
  file(COPY ${CMAKE_CURRENT_LIST_DIR}/assets/models DESTINATION "${PROJECT_SOURCE_DIR}/bin")
endif()

add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.comp"
    )

## iterate each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)

  if(XCODE)
    set(SPIRV "${PROJECT_SOURCE_DIR}/bin/vulkan_guide.app/Contents/Resources/shaders/${FILE_NAME}.spv")
  else()
    set(SPIRV "${PROJECT_SOURCE_DIR}/bin/shaders/${FILE_NAME}.spv")
  endif()
  
  message(STATUS ${GLSL})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )