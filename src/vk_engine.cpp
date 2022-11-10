
#include "vk_engine.h"

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#include <SDL.h>
#include <SDL_syswm.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

#include "vk_types.h"
#include "vk_initializers.h"

void VulkanEngine::init() {
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _windowExtent.width, _windowExtent.height,
                               window_flags);

    // everything went fine
    _isInitialized = true;
}
void VulkanEngine::cleanup() {
    if (_isInitialized) {
        SDL_DestroyWindow(_window);
    }
}

void VulkanEngine::draw() {
    // nothing yet
}

void VulkanEngine::run() {
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT) bQuit = true;
        }

        draw();
    }
}
