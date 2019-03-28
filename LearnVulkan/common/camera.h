//
//  camera.h
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef PUBLIC_CAMERA_H
#define PUBLIC_CAMERA_H

#include <glm/glm.hpp>

#include "window.h"

namespace camera {

class Camera {
 public:
  Camera(const glm::vec3& position = {0.0f, 0.0f, 0.0f},
         const glm::vec3& front = {0.0f, 0.0f, -1.0f},
         const glm::vec3& up = {0.0f, 1.0f, 0.0f},
         float fov = 45.0f,
         float near = 0.1f,
         float far = 100.0f,
         float yaw = -90.0f,
         float pitch = 0.0f,
         float sensitivity = 0.05f);
  void Init(const glm::ivec2& screen_size, const glm::dvec2& cursor_pos);
  void ProcessKey(window::key_map::KeyMap key, float elapsed_time);
  void ProcessCursorMove(double x, double y);
  void ProcessScroll(double y, double min_val, double max_val);

  // This class is neither copyable nor movable
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  const glm::vec3& position()     const { return pos_; }
  const glm::vec3& direction()    const { return front_; }
  const glm::mat4& view_matrix()  const { return view_; }
  const glm::mat4& proj_matrix()  const { return proj_; }

 private:
  int width_, height_;
  float fov_, near_, far_, yaw_, pitch_;
  float last_x_, last_y_, sensitivity_;
  glm::vec3 pos_, front_, up_, right_;
  glm::mat4 view_, proj_;

  void UpdateFrontVector();
  void UpdateRightVector();
  void UpdateViewMatrix();
  void UpdateProjMatrix();
};

} /* namespace camera */

#endif /* PUBLIC_CAMERA_H */
