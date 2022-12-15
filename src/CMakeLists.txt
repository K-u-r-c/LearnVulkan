file(GLOB ENGINE_FILES 
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/camera/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/camera/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/camera/*.hpp")

add_executable(vulkan_guide ${ENGINE_FILES})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if(XCODE)
	set_target_properties(vulkan_guide PROPERTIES
		MACOSX_BUNDLE True
		MACOSX_BUNDLE_GUI_IDENTIFIER my.domain.style.identifier.vulkan_guide
		MACOSX_BUNDLE_BUNDLE_NAME vulkan_guide
		MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
	)
endif()

set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2)

add_dependencies(vulkan_guide Shaders)

if(MSVC)
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

target_link_options(vulkan_guide PRIVATE ${SUBSYSTEM_LINKER_OPTIONS})
