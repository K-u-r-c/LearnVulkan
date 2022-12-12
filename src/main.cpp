#include "vk_engine.h"
#include <iostream>

#if (defined(WIN32) || defined(WIN64) || defined(_WIN32) || \
     defined(_WIN64)) &&                                    \
    defined(NDEBUG)
#include <windows.h>
#endif

int main(int argc, char *argv[]) {
#if (defined(WIN32) || defined(WIN64) || defined(_WIN32) || \
     defined(_WIN64)) &&                                    \
    defined(NDEBUG)
  HWND windowHandle = GetConsoleWindow();
  ShowWindow(windowHandle, SW_HIDE);
#endif

  VulkanEngine engine;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
