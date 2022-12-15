#ifndef A3DB2936_C858_40EC_9A51_5DDCB2896646
#define A3DB2936_C858_40EC_9A51_5DDCB2896646

#include <iostream>

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

class CameraPositionerInterface {
 public:
  virtual ~CameraPositionerInterface() = default;
  virtual glm::mat4 getViewMatrix() const = 0;
  virtual glm::vec3 getPosition() const = 0;
};

class Camera final {
 public:
  explicit Camera(CameraPositionerInterface& positioner)
      : _positioner(&positioner){};

  Camera() = default;
  Camera(const Camera&) = default;
  Camera& operator=(const Camera&) = default;

  glm::mat4 getViewMatrix() const { return _positioner->getViewMatrix(); }
  glm::vec3 getPosition() const { return _positioner->getPosition(); }

 private:
  CameraPositionerInterface* _positioner;
};

class CameraPositioner_FirstPerson final : public CameraPositionerInterface {
 public:
  CameraPositioner_FirstPerson() = default;
  CameraPositioner_FirstPerson(const glm::vec3& pos, const glm::vec3& target,
                               const glm::vec3& up)
      : _cameraPosition(pos),
        _cameraOrientation(glm::lookAt(pos, target, up)),
        _up(up) {}

  void update(double deltaSeconds, const glm::vec2& mousePos,
              bool mousePressed) {
    if (mousePressed) {
      const glm::vec2 delta = mousePos - _mousePos;
      const glm::quat deltaQuat = glm::quat(
          glm::vec3(-_mouseSpeed * delta.y, _mouseSpeed * delta.x, 0.0f));
      _cameraOrientation = deltaQuat * _cameraOrientation;
      _cameraOrientation = glm::normalize(_cameraOrientation);
      setUpVector(_up);
    }
    _mousePos = mousePos;

    glm::mat4 v = glm::mat4_cast(_cameraOrientation);

    const glm::vec3 forward = -glm::vec3(v[0][2], v[1][2], v[2][2]);
    const glm::vec3 right = glm::vec3(v[0][0], v[1][0], v[2][0]);
    const glm::vec3 up = glm::cross(right, forward);

    glm::vec3 accel(0.0f);
    if (_movement._forward) accel += forward;
    if (_movement._backward) accel -= forward;
    if (_movement._left) accel -= right;
    if (_movement._right) accel += right;
    if (_movement._up) accel += up;
    if (_movement._down) accel -= up;

    if (accel == glm::vec3(0)) {
      _moveSpeed -=
          _moveSpeed *
          std::min((1.0f / _damping) * static_cast<float>(deltaSeconds), 1.0f);
    } else {
      _moveSpeed += accel * _acceleration * static_cast<float>(deltaSeconds);
      const float maxSpeed =
          _movement._fastSpeed ? _maxSpeed * _fastCoef : _maxSpeed;
      if (glm::length(_moveSpeed) > maxSpeed)
        _moveSpeed = glm::normalize(_moveSpeed) * maxSpeed;
    }

    _cameraPosition += _moveSpeed * static_cast<float>(deltaSeconds);
  }

  virtual glm::mat4 getViewMatrix() const override {
    const glm::mat4 t = glm::translate(glm::mat4(1.0), -_cameraPosition);
    const glm::mat4 r = glm::mat4_cast(_cameraOrientation);
    return r * t;
  }

  virtual glm::vec3 getPosition() const override { return _cameraPosition; }

  void setUpVector(const glm::vec3& up) {
    const glm::mat4 view = getViewMatrix();
    const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
    _cameraOrientation =
        glm::lookAt(_cameraPosition, _cameraPosition + dir, up);
  }

  void resetMousePosition(const glm::vec2& p) { _mousePos = p; }

 public:
  struct Movement {
    bool _forward{false};
    bool _backward{false};
    bool _left{false};
    bool _right{false};
    bool _up{false};
    bool _down{false};

    bool _fastSpeed{false};
  } _movement;

 public:
  float _mouseSpeed = 4.0f;
  float _acceleration = 150.0f;
  float _damping = 0.2f;
  float _maxSpeed = 10.0f;
  float _fastCoef = 10.0f;

 private:
  glm::vec2 _mousePos = glm::vec2(0.f);
  glm::vec3 _cameraPosition = glm::vec3(0.f, 10.0f, 10.0f);
  glm::quat _cameraOrientation = glm::quat(glm::vec3(0));
  glm::vec3 _moveSpeed = glm::vec3(0.f);
  glm::vec3 _up = glm::vec3(0.f, 0.0f, 1.f);
};

#endif /* A3DB2936_C858_40EC_9A51_5DDCB2896646 */
