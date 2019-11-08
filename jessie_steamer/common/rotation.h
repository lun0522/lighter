//
//  rotation.h
//
//  Created by Pujun Lun on 11/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_ROTATION_H
#define JESSIE_STEAMER_COMMON_ROTATION_H

#include "jessie_steamer/common/timer.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/variant.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
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

class RotationManager {
 public:
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

  template <typename StateType>
  friend absl::optional<rotation::Rotation> rotation::Compute(
      const absl::optional<glm::vec3>& normalized_click_pos,
      RotationManager* rotation_manager);

  // Returns the time since this manager is created.
  float GetReferenceTime() const { return timer_.GetElapsedTimeSinceLaunch(); }

  // Records the time since this manager is created.
  const BasicTimer timer_;

  // Current state.
  State state_ = StopState{};
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_ROTATION_H */
