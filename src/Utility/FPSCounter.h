#include <assert.h>
#include <stdio.h>

class FramesPerSecondCounter {
 public:
  explicit FramesPerSecondCounter(float avgIntervalSec = 0.5f)
      : _avgIntervalSec(avgIntervalSec) {
    assert(_avgIntervalSec > 0.0f);
  }

  bool tick(float deltaSeconds, bool frameRendered = true) {
    if (frameRendered) _numFrames++;
    _accumulatedTime += deltaSeconds;

    if (_accumulatedTime < _avgIntervalSec) return false;

    _currentFPS = static_cast<float>(_numFrames / _accumulatedTime);

    printf("FPS: %.1f\n", _currentFPS);

    _numFrames = 0;
    _accumulatedTime = 0.0;

    return true;
  }

  inline float getFPS() const { return _currentFPS; }

 private:
  const float _avgIntervalSec = 0.5f;
  unsigned int _numFrames = 0;
  double _accumulatedTime = 0.0;
  float _currentFPS = 0.0f;
};
