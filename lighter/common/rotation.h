//
//  rotation.h
//
//  Created by Pujun Lun on 11/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_ROTATION_H
#define LIGHTER_COMMON_ROTATION_H

#include "lighter/common/camera.h"
#include "lighter/common/timer.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/variant.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace common {

// Forward declarations.
class RotationManager;

namespace rotation {

// Describes a rotation.
struct Rotation {
  glm::vec3 axis;
  float angle;
};

// Returns an instance of Rotation if rotation should be performed.
// Otherwise, returns absl::nullopt.
template <typename StateType>
absl::optional<Rotation> Compute(
    const absl::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager);

// This class is used to help visit rotation states and dispatch calls to
// Compute().
class StateVisitor {
 public:
  StateVisitor(const absl::optional<glm::vec3>& normalized_click_pos,
               RotationManager* rotation_manager)
      : normalized_click_pos_{normalized_click_pos},
        rotation_manager_{rotation_manager} {}

  template<typename StateType>
  absl::optional<Rotation> operator()(const StateType& state) {
    return Compute<StateType>(normalized_click_pos_, rotation_manager_);
  }

 private:
  const absl::optional<glm::vec3>& normalized_click_pos_;
  RotationManager* rotation_manager_;
};

} /* namespace rotation */

// This class is used to compute the rotation of 3D objects driven by user
// inputs. The object can be of any shape, and the user only need to provide a
// normalized click position on the object.
class RotationManager {
 public:
  explicit RotationManager(float inertial_rotation_duration)
      : inertial_rotation_duration_{inertial_rotation_duration} {}

  // Returns an instance of rotation::Rotation if rotation should be performed.
  // Otherwise, returns absl::nullopt.
  absl::optional<rotation::Rotation> Compute(
      const absl::optional<glm::vec3>& normalized_click_pos) {
    return absl::visit(
        rotation::StateVisitor{normalized_click_pos, this}, state_);
  }

 private:
  // The object must be in either stop, rotation or inertial rotation state.
  struct StopState {};
  struct RotationState {
    float last_click_time;
    glm::vec3 first_click_pos;
    rotation::Rotation rotation;
  };
  struct InertialRotationState {
    float start_time;
    rotation::Rotation rotation;
  };
  using State = absl::variant<StopState, RotationState, InertialRotationState>;

  // Computes state transition.
  template <typename StateType>
  friend absl::optional<rotation::Rotation> rotation::Compute(
      const absl::optional<glm::vec3>& normalized_click_pos,
      RotationManager* rotation_manager);

  // Returns the time since this manager is created.
  float GetReferenceTime() const { return timer_.GetElapsedTimeSinceLaunch(); }

  // Records the time since this manager is created.
  const BasicTimer timer_;

  // The duration of inertial rotation after the force-driven rotation stops.
  const float inertial_rotation_duration_;

  // Current state.
  State state_ = StopState{};
};

// This class models a sphere that rotates following the user input.
class Sphere {
 public:
  Sphere(const glm::vec3& center, float radius,
         float inertial_rotation_duration);

  // This class is neither copyable nor movable.
  Sphere(const Sphere&) = delete;
  Sphere& operator=(const Sphere&) = delete;

  // Computes whether the user click intersects with the sphere, and returns
  // the coordinate of intersection point in object space if any intersection.
  // Otherwise, returns absl::nullopt.
  absl::optional<glm::vec3> GetIntersection(const Camera& camera,
                                            const glm::vec2& click_ndc) const;

  // Returns how should the sphere be rotated. 'click_ndc' is the user click
  // position in the normalized device coordinate. Because of inertial rotation,
  // the sphere may need to rotate even if the click is not within the sphere.
  absl::optional<rotation::Rotation> ShouldRotate(
      const Camera& camera, const absl::optional<glm::vec2>& click_ndc);

  // Rotates the sphere.
  void Rotate(const rotation::Rotation& rotation);

  // Returns a model matrix for skybox. This is independent of the center and
  // radius of the sphere.
  glm::mat4 GetSkyboxModelMatrix(float scale) const;

  // Accessors.
  const glm::mat4& model_matrix() const { return model_matrix_; }

 private:
  // Describes a ray. 'direction' may not be normalized.
  struct Ray {
    glm::vec3 start;
    glm::vec3 direction;
  };

  // Returns a ray that represents the clicking in the object space.
  Ray GetClickingRay(const Camera& camera, const glm::vec2& click_ndc) const;

  // Center of sphere.
  const glm::vec3 center_;

  // Radius of sphere.
  const float radius_;

  // Model matrix of sphere.
  glm::mat4 model_matrix_;

  // Computes and tracks rotation.
  RotationManager rotation_manager_;
};

} /* namespace common */
} /* namespace lighter */

#endif /* LIGHTER_COMMON_ROTATION_H */
