//
//  camera.h
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef PUBLIC_CAMERA_H
#define PUBLIC_CAMERA_H

#include <glm/glm.hpp>

namespace camera {

enum class CameraMoveDirection {
  kUp, kDown, kLeft, kRight,
};

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
  void ProcessMouseMove(double x, double y);
  void ProcessMouseScroll(double y, double min_val, double max_val);
  void ProcessKeyboardInput(CameraMoveDirection direction, float distance);

  void set_screen_size(int width, int height);
  const glm::vec3& position()     const { return pos_; }
  const glm::vec3& direction()    const { return front_; }
  const glm::mat4& view_matrix()  const { return view_; }
  const glm::mat4& proj_matrix()  const { return proj_; }

 private:
  bool is_first_time_;
  float fov_, near_, far_, yaw_, pitch_;
  float width_, height_, last_x_, last_y_, sensitivity_;
  glm::vec3 pos_, front_, up_, right_;
  glm::mat4 view_, proj_;

  void UpdateFrontVector();
  void UpdateRightVector();
  void UpdateViewMatrix();
  void UpdateProjMatrix();
};

} /* namespace camera */

#endif /* PUBLIC_CAMERA_H */
