// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#ifndef C12F24BE_7752_44A1_B4B1_AA3E1F0F254D
#define C12F24BE_7752_44A1_B4B1_AA3E1F0F254D

#include "vk_types.h"

class VulkanEngine {
   public:
    bool _isInitialized{false};
    int _frameNumber{0};

    VkExtent2D _windowExtent{1700, 900};

    struct SDL_Window* _window{nullptr};

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();

    // run main loop
    void run();
};

#endif /* C12F24BE_7752_44A1_B4B1_AA3E1F0F254D */
