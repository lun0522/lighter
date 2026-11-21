#ifndef LIGHTER_SCENE_SPLINE_H
#define LIGHTER_SCENE_SPLINE_H

#include <cstddef>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace lighter::scene {

// This is the base class of all spline classes. The user should use it
// through derived classes. Note that spline classes determine the way to build
// splines using control points, but do not own control points.
class Spline {
 public:
  // Index span [start_spline_index, end_spline_index) in 'spline_points()'
  // connecting 'start_control_point' to 'end_control_point'.
  struct SegmentRange {
    int start_control_point = 0;
    int end_control_point = 0;
    size_t start_spline_index = 0;
    size_t end_spline_index = 0;
  };

  // This class is neither copyable nor movable.
  Spline(const Spline&) = delete;
  Spline& operator=(const Spline&) = delete;

  virtual ~Spline() = default;

  // Populates 'spline_points_' with the spline built from 'control_points'.
  // Previous content of 'spline_points_' will be discarded.
  virtual void BuildSpline(std::span<const glm::vec3> control_points) = 0;

  // Accessors.
  const std::vector<glm::vec3>& spline_points() const { return spline_points_; }
  const std::vector<SegmentRange>& segment_ranges() const {
    return segment_ranges_;
  }

 protected:
  Spline() = default;

  // Accessors.
  std::vector<glm::vec3>* mutable_splines() { return &spline_points_; }
  std::vector<SegmentRange>* mutable_segment_ranges() {
    return &segment_ranges_;
  }

 private:
  // Positions of spline points.
  std::vector<glm::vec3> spline_points_;

  // Control-point index mapping for each tessellated segment.
  std::vector<SegmentRange> segment_ranges_;
};

// This class provides functions to build a bezier spline recursively:
// http://www.cs.cornell.edu/courses/cs4620/2017sp/slides/16spline-curves.pdf
class BezierSpline : public Spline {
 public:
  // Projects a control or emitted spline point (for example onto a sphere).
  using ProjectPoint = std::function<glm::vec3(const glm::vec3& p)>;

  // Returns whether the spline segment can be considered smooth given 4 control
  // points. This is used for determining whether the recursion should stop.
  using IsSmooth =
      std::function<bool(const glm::vec3& p0, const glm::vec3& p1,
                         const glm::vec3& p2, const glm::vec3& p3)>;

  // If the depth of recursion reaches 'max_recursion_depth', or if 'is_smooth'
  // returns true, the recursion will stop.
  BezierSpline(int max_recursion_depth, IsSmooth&& is_smooth,
               ProjectPoint&& project_point = {});

  // This class is neither copyable nor movable.
  BezierSpline(const BezierSpline&) = delete;
  BezierSpline& operator=(const BezierSpline&) = delete;

 protected:
  // This function is recursively called to interpolate spline points. When
  // recursion stop conditions are met, 'p0' (projected if 'project_point_' is
  // set) will be added to 'spline_points_'.
  void Tessellate(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                  const glm::vec3& p3, int recursion_depth);

 private:
  // Maximum recursion depth of calling Tessellate().
  const int max_recursion_depth_;

  // Returns whether the spline segment can be considered smooth.
  const IsSmooth is_smooth_;

  // Optional projection applied to emitted spline points.
  const ProjectPoint project_point_;
};

// This class is used to build centripetal Catmull-Rom splines, so that we can
// guarantee the spline passes control points without cusps or self-loops.
class CatmullRomSpline : public BezierSpline {
 public:
  // We cannot build the spline with less than 3 control points.
  static const int kMinNumControlPoints;

  // Returns a centripetal Catmull-Rom spline projected onto the unit sphere.
  static std::unique_ptr<Spline> GetOnSphereSpline(int max_recursion_depth,
                                                   float roughness);

  // Inherits constructor.
  using BezierSpline::BezierSpline;

  // This class is neither copyable nor movable.
  CatmullRomSpline(const CatmullRomSpline&) = delete;
  CatmullRomSpline& operator=(const CatmullRomSpline&) = delete;

  // Overrides.
  void BuildSpline(std::span<const glm::vec3> control_points) override;

 private:
  // Converts Catmull-Rom spline control points to bezier spline control points,
  // and builds the spline.
  void Tessellate(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                  const glm::vec3& p3);
};

// This class is used to handle user interactions with control points. The user
// can build any kind of splines and pass to this class, and manipulate the
// spline through it.
class SplineEditor {
 public:
  SplineEditor(int min_num_control_points, int max_num_control_points,
               std::vector<glm::vec3>&& initial_control_points,
               std::unique_ptr<Spline>&& spline);

  // This class is neither copyable nor movable.
  SplineEditor(const SplineEditor&) = delete;
  SplineEditor& operator=(const SplineEditor&) = delete;

  // Returns whether we can insert any more control points.
  bool CanInsertControlPoint() const;

  // Returns the insertion index that splits the spline segment closest to
  // 'position', optionally filtering to segments visible per
  // 'is_point_visible'. Returns std::nullopt if no eligible segment is found.
  std::optional<int> FindClosestInsertionIndex(
      const glm::vec3& position,
      const std::function<bool(const glm::vec3&)>& is_point_visible = {}) const;

  // Inserts a new control point at 'index'.
  bool InsertControlPoint(int index, const glm::vec3& position);

  // Updates the control point at 'index'.
  void UpdateControlPoint(int index, const glm::vec3& position);

  // Removes the control point at 'index'.
  bool RemoveControlPoint(int index);

  // Accessors.
  const std::vector<glm::vec3>& control_points() const {
    return control_points_;
  }
  const std::vector<glm::vec3>& spline_points() const {
    return spline_->spline_points();
  }

 private:
  // Re-generates all spline points. This should be called whenever any control
  // point changes.
  void RebuildSpline();

  // Minimum/maximum number of control points.
  const int min_num_control_points_;
  const int max_num_control_points_;

  // Positions of control points.
  std::vector<glm::vec3> control_points_;

  // Determines how to build the spline from control points.
  std::unique_ptr<Spline> spline_;
};

}  // namespace lighter::scene

#endif  // LIGHTER_SCENE_SPLINE_H
