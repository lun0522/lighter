//
//  camera.h
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_CAMERA_H
#define JESSIE_STEAMER_COMMON_CAMERA_H

#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

// A prospective camera model. Users should use subclasses of it.
class Camera {
 public:
  // Configurations used to initialize a camera.
  struct Config {
    float near = 0.1f;
    float far = 100.0f;
    float field_of_view = 45.0f;
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 position{0.0f, 0.0f, -1.0f};
    glm::vec3 look_at{0.0f, 0.0f, 0.0f};
  };

  // This class is neither copyable nor movable.
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  // Accessors.
  const glm::vec3& position() const { return pos_; }
  const glm::vec3& direction() const { return front_; }
  const glm::mat4& view() const { return view_; }
  const glm::mat4& projection() const { return proj_; }

 protected:
  explicit Camera(const Config& config);

  // Updates the field of view and projection matrix.
  void UpdateFieldOfView(float fov);

  // Updates the screen size and projection matrix.
  void UpdateScreenSize(const glm::ivec2& screen_size);

  // Moves the position of camera by 'offset' and updates the view matrix.
  void UpdatePosition(const glm::vec3& offset);

  // Updates the front and right vector, and view matrix.
  void UpdateDirection(const glm::vec3& front);

  // Accessors.
  float field_of_view() const { return fov_; }
  const glm::vec3& front() const { return front_; }
  const glm::vec3& right() const { return right_; }

  // Distance of the near plane.
  const float near_;

  // Distance of the far plane.
  const float far_;

  // Up vector.
  const glm::vec3 up_;

 private:
  // Updates the projection matrix.
  void UpdateProjection();

  // Updates the view matrix.
  void UpdateView();

  // Field of view.
  float fov_;

  // Size of screen.
  glm::ivec2 screen_size_;

  // Position.
  glm::vec3 pos_;

  // Front vector.
  glm::vec3 front_;

  // Right vector.
  glm::vec3 right_;

  // View matrix.
  glm::mat4 view_;

  // Projection matrix.
  glm::mat4 proj_;
};

// A prospective camera model with cursor, scroll and keyboard control.
// Users are responsible to call 'SetActivity' to activate the camera, and call
// 'Calibrate' after a screen is created and whenever it is resized.
class UserControlledCamera : public Camera {
 public:
  // Users may use these keys to control the camera.
  enum class ControlKey { kUp, kDown, kLeft, kRight };

  // Configurations used to initialize the control.
  struct ControlConfig {
    float move_speed = 10.0f;
    float turn_speed = 0.001f;
    bool lock_center = false;
  };

  UserControlledCamera(const Config& config,
                       const ControlConfig& control_config);

  // This class is neither copyable nor movable.
  UserControlledCamera(const UserControlledCamera&) = delete;
  UserControlledCamera& operator=(const UserControlledCamera&) = delete;

  // Calibrates the camera with screen size and cursor position.
  // This should be called after the screen is created or resized.
  void Calibrate(const glm::ivec2& screen_size, const glm::dvec2& cursor_pos);

  // Informs the camera that the cursor has been moved to position ('x', 'y').
  // The camera will point to a different direction according to it, while
  // the degree turned depends on 'turn_speed_'.
  // If the camera is in the lock center mode, it would not respond to this.
  void DidMoveCursor(double x, double y);

  // Informs the camera that the scroll input has changed by 'delta'.
  // The camera will change the field of view by 'delta', bound by
  // ['min_val', 'max_val'], which produces the effects of zoom in/out.
  void DidScroll(double delta, double min_val, double max_val);

  // Informs the camera that 'key' has been pressed.
  // The camera will move to a different position depending on the key, while
  // the distance traveled is determined by 'elapsed_time' and 'move_speed_'.
  void DidPressKey(ControlKey key, float elapsed_time);

  // Modifiers.
  void SetActivity(bool active) { is_active_ = active; }

 private:
  // Whether the camera responds to user inputs.
  bool is_active_ = false;

  // The current position of cursor in the screen coordinate.
  glm::dvec2 cursor_pos_;

  // The movement speed of camera (forward/backward/left/right).
  const float move_speed_;

  // The turning speed of camera (pitch/yaw).
  const float turn_speed_;

  // The camera should always face the center in the lock center mode.
  const glm::vec3 center_;

  // Whether or not to use the lock center mode.
  const bool lock_center_;

  // Pitch in radians.
  float pitch_;

  // Yaw in radians.
  float yaw_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_CAMERA_H */
