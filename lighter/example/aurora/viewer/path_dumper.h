#ifndef LIGHTER_EXAMPLE_AURORA_VIEWER_PATH_DUMPER_H
#define LIGHTER_EXAMPLE_AURORA_VIEWER_PATH_DUMPER_H

#include <memory>
#include <span>

#include "lighter/example/aurora/editor/path.h"
#include "lighter/example/aurora/viewer/path_renderer.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/scene/camera.h"
#include "lighter/scene/distance_field.h"

namespace lighter::example::aurora {

// Rasterizes 3D aurora splines and runs compute post-processing + Jump Flooding
// distance field generation inside a single Frame Graph.
class PathDumper {
 public:
  // 'paths_image_dimension' must be a power of 2.
  PathDumper(const renderer::ir::Renderer& renderer, int paths_image_dimension);

  // This class is neither copyable nor movable.
  PathDumper(const PathDumper&) = delete;
  PathDumper& operator=(const PathDumper&) = delete;

  void DumpAuroraPaths(renderer::ir::Renderer& renderer,
                       const scene::Camera& camera,
                       std::span<const SplinePathView> spline_paths);

  const renderer::ir::Image& aurora_paths_image() const {
    return *paths_image_;
  }
  const renderer::ir::Image& distance_field_image() const {
    return *distance_field_image_;
  }

 private:
  std::unique_ptr<renderer::ir::Image> paths_image_;
  std::unique_ptr<renderer::ir::Image> distance_field_image_;
  PathRenderer2D path_renderer_;
  scene::DistanceFieldGenerator distance_field_generator_;
};

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_VIEWER_PATH_DUMPER_H
