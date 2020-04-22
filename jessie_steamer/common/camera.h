//
//  camera.h
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_CAMERA_H
#define JESSIE_STEAMER_COMMON_CAMERA_H

#include <functional>
#include <memory>

#ifdef USE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif /* USE_VULKAN */
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

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

  // Updates right and front vectors. 'right' does not need to be normalized.
  Camera& SetRight(const glm::vec3& right);

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
  struct PersConfig {
    float field_of_view;
    float aspect_ratio;
  };

  PerspectiveCamera(const Camera::Config& config,
                    const PersConfig& pers_config)
      : Camera{config}, aspect_ratio_{pers_config.aspect_ratio},
        fov_{pers_config.field_of_view} {}

  // This class is neither copyable nor movable.
  PerspectiveCamera(const PerspectiveCamera&) = delete;
  PerspectiveCamera& operator=(const PerspectiveCamera&) = delete;

  // Updates the field of view.
  PerspectiveCamera& SetFieldOfView(float fov);

  // Overrides.
  glm::mat4 GetProjectionMatrix() const override;

  // Accessors.
  float field_of_view() const { return fov_; }

 private:
  // Aspect ratio of field of view.
  const float aspect_ratio_;

  // Field of view.
  float fov_;
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
    return {/*view_width=*/2.0f, /*aspect_ratio=*/1.0f};
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

// A camera model with cursor, scroll and keyboard control.
// The camera is not active after construction. The user should call
// SetActivity() to activate/deactivate it. The user should also call
// SetCursorPos() after a window is created and whenever it is resized.
class UserControlledCamera {
 public:
  // The user may use these keys to control the camera.
  enum class ControlKey { kUp, kDown, kLeft, kRight };

  // Configurations used to initialize the control.
  struct ControlConfig {
    float move_speed = 10.0f;
    float turn_speed = 0.001f;
  };

  UserControlledCamera(const ControlConfig& control_config,
                       std::unique_ptr<Camera>&& camera)
      : move_speed_{control_config.move_speed},
        turn_speed_{control_config.turn_speed},
        camera_{std::move(camera)} {
    Reset();
  }

  // This class is neither copyable nor movable.
  UserControlledCamera(const UserControlledCamera&) = delete;
  UserControlledCamera& operator=(const UserControlledCamera&) = delete;

  // Directly modifies states of the underlying camera object. Internal states
  // of this class will be reset after the modification.
  void SetInternalStates(const std::function<void(Camera*)>& operation);

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
  void DidPressKey(ControlKey key, float elapsed_time);

  // Modifiers.
  void SetActivity(bool active) { is_active_ = active; }

  // Accessors.
  const Camera& camera() const { return *camera_; }

 private:
  // Resets reference vectors and angles, and turns to the coordinate system
  // built with the current camera up and front vectors.
  void Reset();

  // Moving speed of camera (forward/backward/left/right).
  const float move_speed_;

  // Turning speed of camera (pitch/yaw).
  const float turn_speed_;

  // Whether the camera responds to user inputs.
  bool is_active_ = false;

  // Camera model.
  std::unique_ptr<Camera> camera_;

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

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_CAMERA_H */
