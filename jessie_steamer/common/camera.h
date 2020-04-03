//
//  camera.h
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_CAMERA_H
#define JESSIE_STEAMER_COMMON_CAMERA_H

#include <memory>

#include "third_party/absl/types/optional.h"
#ifdef USE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif /* USE_VULKAN */
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

// A camera model. Subclasses should override the projection() method.
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

  // Moves the position of camera by 'offset' and updates the view matrix.
  void UpdatePosition(const glm::vec3& offset);

  // Updates the front and right vector, and view matrix. 'front' does not need
  // to be normalized.
  void UpdateDirection(const glm::vec3& front);

  // Returns a view matrix that can be used for rendering skybox.
  glm::mat4 GetSkyboxViewMatrix() const;

  // Accessors.
  const glm::vec3& position() const { return pos_; }
  const glm::vec3& front() const { return front_; }
  const glm::vec3& right() const { return right_; }
  const glm::mat4& view() const { return view_; }
  virtual const glm::mat4& projection() const = 0;

 protected:
  explicit Camera(const Config& config);

  // Distance of the near plane.
  const float near_;

  // Distance of the far plane.
  const float far_;

  // Up vector.
  const glm::vec3 up_;

 private:
  // Updates the view matrix.
  void UpdateView();

  // Position.
  glm::vec3 pos_;

  // Front vector.
  glm::vec3 front_;

  // Right vector.
  glm::vec3 right_;

  // View matrix.
  glm::mat4 view_;
};

// A perspective camera model.
class PerspectiveCamera : public Camera {
 public:
  // Configurations used to control perspective projection. 'field_of_view' is
  // measured in degrees.
  struct PersConfig {
    float field_of_view;
    float fov_aspect_ratio;
  };

  PerspectiveCamera(const Camera::Config& config,
                    const PersConfig& pers_config);

  // This class is neither copyable nor movable.
  PerspectiveCamera(const PerspectiveCamera&) = delete;
  PerspectiveCamera& operator=(const PerspectiveCamera&) = delete;

  // Updates the field of view and projection matrix.
  void UpdateFieldOfView(float fov);

  // Accessors.
  float field_of_view() const { return fov_; }
  const glm::mat4& projection() const override { return proj_; }

 private:
  // Updates the projection matrix.
  void UpdateProjection();

  // Field of view.
  float fov_;

  // Aspect ratio of field of view.
  const float fov_aspect_ratio_;

  // Projection matrix.
  glm::mat4 proj_;
};

// An orthographic camera model.
class OrthographicCamera : public Camera {
 public:
  struct OrthoConfig {
    float view_width;
    float aspect_ratio;
  };

  OrthographicCamera(const Camera::Config& config,
                     const OrthoConfig& ortho_config);

  // This class is neither copyable nor movable.
  OrthographicCamera(const OrthographicCamera&) = delete;
  OrthographicCamera& operator=(const OrthographicCamera&) = delete;

  // Updates the width of view and projection matrix, while keeping the aspect
  // ratio unchanged.
  void UpdateViewWidth(float view_width);

  // Accessors.
  float view_width() const { return view_width_; }
  const glm::mat4& projection() const override { return proj_; }

 private:
  // Updates the projection matrix.
  void UpdateProjection();

  // Aspect ratio of view.
  const float aspect_ratio_;

  // Width of view.
  float view_width_;

  // Projection matrix.
  glm::mat4 proj_;
};

// A camera model with cursor, scroll and keyboard control.
// Users are responsible to call SetActivity() to activate the camera, and call
// SetCursorPos() after a window is created and whenever it is resized.
class UserControlledCamera {
 public:
  // Users may use these keys to control the camera.
  enum class ControlKey { kUp, kDown, kLeft, kRight };

  // Configurations used to initialize the control. If 'lock_center' has value,
  // the camera will always face it, and will not respond to cursor movements.
  struct ControlConfig {
    float move_speed = 10.0f;
    float turn_speed = 0.001f;
    absl::optional<glm::vec3> lock_center;
  };

  UserControlledCamera(const ControlConfig& control_config,
                       std::unique_ptr<Camera>&& camera);

  // This class is neither copyable nor movable.
  UserControlledCamera(const UserControlledCamera&) = delete;
  UserControlledCamera& operator=(const UserControlledCamera&) = delete;

  // Sets the cursor position. If the user care about the mouse movement, this
  // should be called after the window is created or resized.
  void SetCursorPos(const glm::dvec2& cursor_pos) { cursor_pos_ = cursor_pos; }

  // Informs the camera that the cursor has been moved to position ('x', 'y').
  // The camera will point to a different direction according to it, while
  // the degree turned depends on 'turn_speed_'.
  // If the camera is in the lock center mode, it would not respond to this.
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
  void DidPressKey(ControlKey key, float elapsed_time);

  // Modifiers.
  void SetActivity(bool active) { is_active_ = active; }

  // Accessors.
  const Camera& camera() const { return *camera_; }

 private:
  // Updates camera direction if center is not locked.
  void UpdateDirectionIfNeeded();

  // Whether the camera responds to user inputs.
  bool is_active_ = false;

  // Camera model.
  std::unique_ptr<Camera> camera_;

  // Current position of cursor in the screen coordinate.
  glm::dvec2 cursor_pos_;

  // Movement speed of camera (forward/backward/left/right).
  const float move_speed_;

  // Turning speed of camera (pitch/yaw).
  const float turn_speed_;

  // If has value, the camera will always face the center.
  const absl::optional<glm::vec3> lock_center_;

  // Pitch in radians.
  float pitch_;

  // Yaw in radians.
  float yaw_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_CAMERA_H */
