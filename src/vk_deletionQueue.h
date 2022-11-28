#ifndef D1C6ECBC_BEB2_49E8_8B46_03786A22C95C
#define D1C6ECBC_BEB2_49E8_8B46_03786A22C95C

#include <functional>
#include <deque>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()>&& func) {
    deletors.push_back(std::move(func));
  }

  void flush() {
    // Reverse iterate the deletion queue to eqecute all the function
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)();  // Function call
    }
  }
};

#endif /* D1C6ECBC_BEB2_49E8_8B46_03786A22C95C */
