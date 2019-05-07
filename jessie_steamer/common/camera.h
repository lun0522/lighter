//
//  camera.h
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_CAMERA_H
#define JESSIE_STEAMER_COMMON_CAMERA_H

#include "jessie_steamer/common/window.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

class Camera {
 public:
  struct Config {
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 pos{0.0f, 0.0f, -1.0f};
    glm::vec3 look_at{0.0f, 0.0f, 0.0f};
    float fov = 45.0f;
    float near = 0.1f;
    float far = 100.0f;
    float move_speed = 10.0f;
    float turn_speed = 0.001f;
    bool lock_look_at = false;
  };

  Camera() = default;

  // This class is neither copyable nor movable
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  void Init(const Config& config);
  void Calibrate(const glm::ivec2& screen_size,
                 const glm::dvec2& cursor_pos);
  void Activate();
  void Deactivate();
  void ProcessKey(Window::KeyMap key, float elapsed_time);
  void ProcessCursorMove(double x, double y);
  void ProcessScroll(double y, double min_val, double max_val);

  const glm::vec3& position()   const { return pos_; }
  const glm::vec3& direction()  const { return front_; }
  const glm::mat4& view()       const { return view_; }
  const glm::mat4& projection() const { return proj_; }

 private:
  void UpdateFront();
  void UpdateRight();
  void UpdateView();
  void UpdateProj();

  bool is_active_ = false;
  bool lock_center_;
  float fov_, near_, far_, yaw_, pitch_;
  float move_speed_, turn_speed_;
  glm::ivec2 screen_size_;
  glm::dvec2 cursor_pos_;
  glm::vec3 pos_, front_, up_, right_, center_;
  glm::mat4 view_, proj_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_CAMERA_H */
