//
//  spline.h
//
//  Created by Pujun Lun on 11/30/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_SPLINE_H
#define JESSIE_STEAMER_COMMON_SPLINE_H

#include <functional>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

class Spline {
 public:
  // This class is neither copyable nor movable.
  Spline(const Spline&) = delete;
  Spline& operator=(const Spline&) = delete;

  virtual ~Spline() = default;

  virtual void BuildSpline(const std::vector<glm::vec3>& control_points) = 0;

  int GetSucceedingControlPointIndex(int spline_point_index) const;

  const std::vector<glm::vec3>& spline_points() const { return spline_points_; }

 protected:
  Spline() = default;

  std::vector<glm::vec3> spline_points_;
  std::vector<int> control_point_precedence_;
};

class BezierSpline : public Spline {
 public:
  using GetMiddlePoint = std::function<glm::vec3(const glm::vec3& p0,
                                                 const glm::vec3& p1)>;

  using IsSmooth = std::function<bool(const glm::vec3& p0,
                                      const glm::vec3& p1,
                                      const glm::vec3& p2,
                                      const glm::vec3& p3)>;

  BezierSpline(int max_recursion_depth,
               GetMiddlePoint&& get_middle_point,
               IsSmooth&& is_smooth);

  // This class is neither copyable nor movable.
  BezierSpline(const BezierSpline&) = delete;
  BezierSpline& operator=(const BezierSpline&) = delete;

 protected:
  void Tessellate(const glm::vec3& p0,
                  const glm::vec3& p1,
                  const glm::vec3& p2,
                  const glm::vec3& p3,
                  int recursion_depth);

 private:
  const int max_recursion_depth_;
  const GetMiddlePoint get_middle_point_;
  const IsSmooth is_smooth_;
};

class CatmullRomSpline : public BezierSpline {
 public:
  static const int kMinNumControlPoints;

  static std::unique_ptr<Spline> GetOnSphereSpline(
      int max_recursion_depth, float smoothness);

  // Inherits constructor.
  using BezierSpline::BezierSpline;

  // This class is neither copyable nor movable.
  CatmullRomSpline(const CatmullRomSpline&) = delete;
  CatmullRomSpline& operator=(const CatmullRomSpline&) = delete;

  void BuildSpline(const std::vector<glm::vec3>& control_points) override;

 private:
  void Tessellate(const glm::vec3& p0,
                  const glm::vec3& p1,
                  const glm::vec3& p2,
                  const glm::vec3& p3);
};

class SplineEditor {
 public:
  SplineEditor(int min_num_control_points,
               int max_num_control_points,
               std::vector<glm::vec3>&& initial_control_points,
               std::unique_ptr<Spline>&& spline);

  // This class is neither copyable nor movable.
  SplineEditor(const SplineEditor&) = delete;
  SplineEditor& operator=(const SplineEditor&) = delete;

  absl::optional<int> FindClickedControlPoint(
      const glm::vec3& click_pos, float control_point_radius);

  bool AddControlPoint(const glm::vec3& click_pos,
                       float max_distance_from_spline);

  void UpdateControlPoint(int index, const glm::vec3& new_pos);

  bool RemoveControlPoint(int index);

  const std::vector<glm::vec3>& control_points() const {
    return control_points_;
  }
  const std::vector<glm::vec3>& spline_points() const {
    return spline_->spline_points();
  }

 private:
  void RebuildSpline();

  const int min_num_control_points_;
  const int max_num_control_points_;
  std::vector<glm::vec3> control_points_;
  std::unique_ptr<Spline> spline_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_SPLINE_H */
