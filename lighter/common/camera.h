//
//  camera.h
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_CAMERA_H
#define LIGHTER_COMMON_CAMERA_H

#include <memory>
#include <optional>

#include "third_party/absl/functional/function_ref.h"
#include "third_party/absl/memory/memory.h"
#ifdef USE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif  // USE_VULKAN
#include "third_party/glm/glm.hpp"

namespace lighter::common {

// A camera model. Subclasses must override GetProjectionMatrix().
class Camera {
 public:
  // Configurations used to initialize a camera.
  struct Config {
    float near = 0.1f;
    float far = 100.0f;
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 position{0.0f, 0.0f, 1.0f};
    glm::vec3 look_at{0.0f, 0.0f, 0.0f};
  };

  // This class is neither copyable nor movable.
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  virtual ~Camera() = default;

  // Moves the position of camera by 'offset'.
  Camera& UpdatePositionByOffset(const glm::vec3& offset);

  // Moves the camera to 'position'.
  Camera& SetPosition(const glm::vec3& position);

  // Updates the up vector. 'up' does not need to be normalized.
  Camera& SetUp(const glm::vec3& up);

  // Updates front and right vectors. 'front' does not need to be normalized.
  Camera& SetFront(const glm::vec3& front);

  // Returns the view matrix.
  glm::mat4 GetViewMatrix() const;

  // Returns a view matrix that can be used for rendering skybox.
  glm::mat4 GetSkyboxViewMatrix() const {
    return glm::mat4{glm::mat3{GetViewMatrix()}};
  }

  // Returns the projection matrix.
  virtual glm::mat4 GetProjectionMatrix() const = 0;

  // Accessors.
  const glm::vec3& position() const { return pos_; }
  const glm::vec3& up() const { return up_; }
  const glm::vec3& front() const { return front_; }
  const glm::vec3& right() const { return right_; }

 protected:
  explicit Camera(const Config& config)
      : near_{config.near}, far_{config.far},
        pos_{config.position}, up_{glm::normalize(config.up)} {
    SetFront(config.look_at - pos_);
  }

  // Distance to the near plane.
  const float near_;

  // Distance to the far plane.
  const float far_;

 private:
  // Position.
  glm::vec3 pos_;

  // Up vector.
  glm::vec3 up_;

  // Front vector.
  glm::vec3 front_;

  // Right vector.
  glm::vec3 right_;
};

// A perspective camera model.
class PerspectiveCamera : public Camera {
 public:
  // Configurations used to control perspective projection. 'field_of_view' is
  // measured in degrees.
  struct FrustumConfig {
    float field_of_view_y;
    float aspect_ratio;
  };

  // Used for computing the direction of a view ray when doing ray tracing. If
  // the coordinate of a pixel is (x, y), where both x and y are in range
  // [-1, 1], it will be used to trace the ray shooting from the camera position
  // in the direction: 'right' * x + 'up' * y + 'front'.
  struct RayTracingParams {
    glm::vec3 up;
    glm::vec3 front;
    glm::vec3 right;
  };

  PerspectiveCamera(const Camera::Config& config,
                    const FrustumConfig& frustum_config)
      : Camera{config}, aspect_ratio_{frustum_config.aspect_ratio},
        fovy_{frustum_config.field_of_view_y} {}

  // This class is neither copyable nor movable.
  PerspectiveCamera(const PerspectiveCamera&) = delete;
  PerspectiveCamera& operator=(const PerspectiveCamera&) = delete;

  // Updates the field of view in Y-axis.
  PerspectiveCamera& SetFieldOfViewY(float fovy);

  // Returns parameters used for ray tracing.
  RayTracingParams GetRayTracingParams() const;

  // Overrides.
  glm::mat4 GetProjectionMatrix() const override;

  // Accessors.
  float field_of_view_y() const { return fovy_; }

 private:
  // Aspect ratio of field of view.
  const float aspect_ratio_;

  // Field of view in Y-axis.
  float fovy_;
};

// An orthographic camera model.
class OrthographicCamera : public Camera {
 public:
  // Configurations used to control orthographic projection.
  struct OrthoConfig {
    float view_width;
    float aspect_ratio;
  };

  // Returns a OrthoConfig for rendering a fullscreen squad.
  static OrthoConfig GetFullscreenConfig() {
    return {.view_width = 2.0f, .aspect_ratio = 1.0f};
  }

  OrthographicCamera(const Camera::Config& config,
                     const OrthoConfig& ortho_config)
      : Camera{config}, aspect_ratio_{ortho_config.aspect_ratio},
        view_width_{ortho_config.view_width} {}

  // This class is neither copyable nor movable.
  OrthographicCamera(const OrthographicCamera&) = delete;
  OrthographicCamera& operator=(const OrthographicCamera&) = delete;

  // Updates the width of view, while keeping the aspect ratio unchanged.
  OrthographicCamera& SetViewWidth(float view_width);

  // Overrides.
  glm::mat4 GetProjectionMatrix() const override;

  // Accessors.
  float view_width() const { return view_width_; }

 private:
  // Aspect ratio of view.
  const float aspect_ratio_;

  // Width of view.
  float view_width_;
};

namespace camera_control {

// The user may use these keys to control the camera.
enum class Key { kUp, kDown, kLeft, kRight };

// Configurations used to initialize the control.
struct Config {
  // If the user keeps pressing the key for 1 second, the position of camera
  // will change by 'move_speed'. If 'lock_center' has value, it will be
  // measured by radians.
  float move_speed = 10.0f;

  // If the user moves the cursor by 1 pixel, the direction of camera will
  // change by 'turn_speed' measured in radians.
  float turn_speed = 0.0005f;

  // When the user presses on keys, if this has value, the camera will move
  // with no constraints. Otherwise, the camera will move on the surface of a
  // sphere, whose center is 'lock_center', and radius is the distance between
  // the initial position of camera and the 'lock_center' point.
  std::optional<glm::vec3> lock_center;
};

}  // namespace camera_control

// A camera model with cursor, scroll and keyboard control.
// The camera is not active after construction. The user should call
// SetActivity() to activate/deactivate it. The user should also call
// SetCursorPos() after a window is created and whenever it is resized.
template <typename CameraType>
class UserControlledCamera {
 public:
  // This class is neither copyable nor movable.
  UserControlledCamera(const UserControlledCamera&) = delete;
  UserControlledCamera& operator=(const UserControlledCamera&) = delete;

  virtual ~UserControlledCamera() = default;

  // Directly modifies states of the underlying camera object. Internal states
  // of this class will be reset after the modification.
  void SetInternalStates(absl::FunctionRef<void(CameraType*)> operation);

  // Sets the cursor position. If the user care about the mouse movement, this
  // should be called after the window is created or resized.
  void SetCursorPos(const glm::dvec2& cursor_pos) { cursor_pos_ = cursor_pos; }

  // Informs the camera that the cursor has been moved to position ('x', 'y').
  // The camera will point to a different direction according to it, while
  // the degree turned depends on 'turn_speed_'.
  void DidMoveCursor(double x, double y);

  // Informs the camera that the scroll input has changed by 'delta', bound by
  // ['min_val', 'max_val'], and returns if anything is changed. For the
  // perspective camera, this changes the field of view by 'delta'. For the
  // orthographic camera, this changes the width of view by 'delta'.
  // This will produce the effects of zooming in/out.
  bool DidScroll(double delta, double min_val, double max_val);

  // Informs the camera that 'key' has been pressed.
  // The camera will move to a different position depending on the key, while
  // the distance traveled is determined by 'elapsed_time' and 'move_speed_'.
  void DidPressKey(camera_control::Key key, float elapsed_time);

  // Modifiers.
  void SetActivity(bool active) { is_active_ = active; }

  // Accessors.
  const CameraType& camera() const { return *camera_; }

protected:
  UserControlledCamera(const camera_control::Config& control_config,
                       std::unique_ptr<CameraType>&& camera)
      : control_config_{control_config}, camera_{std::move(camera)} {
    ResetAngles();
  }

 private:
  // Resets reference vectors and angles, and turns to the coordinate system
  // built with the current camera up and front vectors.
  void ResetAngles();

  // Controls the way the camera moves and turns.
  const camera_control::Config control_config_;

  // Whether the camera responds to user inputs.
  bool is_active_ = false;

  // Camera model.
  std::unique_ptr<CameraType> camera_;

  // Current position of cursor in the screen coordinate.
  glm::dvec2 cursor_pos_;

  // These reference vectors and the camera up vector form a coordinate system,
  // and we calculate pitch and yaw angles based on it.
  glm::vec3 ref_front_;
  glm::vec3 ref_left_;

  // Pitch in radians.
  float pitch_;

  // Yaw in radians.
  float yaw_;
};

class UserControlledPerspectiveCamera
    : public UserControlledCamera<PerspectiveCamera> {
 public:
  static std::unique_ptr<UserControlledPerspectiveCamera> Create(
      const camera_control::Config& control_config,
      const Camera::Config& camera_config,
      const PerspectiveCamera::FrustumConfig& frustum_config) {
    return absl::WrapUnique(new UserControlledPerspectiveCamera {
        control_config,
        std::make_unique<PerspectiveCamera>(camera_config, frustum_config)});
  }

  protected:
   // Inherits constructor.
   using UserControlledCamera<PerspectiveCamera>::UserControlledCamera;
};

class UserControlledOrthographicCamera
    : public UserControlledCamera<OrthographicCamera> {
 public:
  static std::unique_ptr<UserControlledOrthographicCamera> Create(
      const camera_control::Config& control_config,
      const Camera::Config& camera_config,
      const OrthographicCamera::OrthoConfig& ortho_config) {
    return absl::WrapUnique(new UserControlledOrthographicCamera{
        control_config,
        std::make_unique<OrthographicCamera>(camera_config, ortho_config)});
  }

 protected:
  // Inherits constructor.
  using UserControlledCamera<OrthographicCamera>::UserControlledCamera;
};

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_CAMERA_H
