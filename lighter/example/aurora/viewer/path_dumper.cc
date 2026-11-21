//
//  path_dumper.cc
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/viewer/path_dumper.h"

#include <array>
#include <format>

#include "lighter/base/image.h"
#include "lighter/base/timer.h"
#include "lighter/base/util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::example::aurora {
namespace {

using renderer::ir::AccessType;
using renderer::ir::ImageUsage;
using renderer::ir::MultisamplingMode;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;

}  // namespace

PathDumper::PathDumper(const Renderer& renderer, int paths_image_dimension)
    : path_renderer_{glm::ivec2{paths_image_dimension},
                     MultisamplingMode::kBest},
      distance_field_generator_{glm::ivec2{paths_image_dimension}} {
  ASSERT_TRUE(base::util::IsPowerOf2(paths_image_dimension),
              std::format("'paths_image_dimension' is expected to be power "
                          "of 2, while {} provided",
                          paths_image_dimension));

  const base::Image::Dimension bw_dim{paths_image_dimension,
                                      paths_image_dimension,
                                      base::image::kBwImageChannel};
  paths_image_ = renderer.CreateColorImage(
      "AuroraPaths", bw_dim, MultisamplingMode::kNone,
      /*high_precision=*/false,
      std::array{
          ImageUsage::GetLinearAccessInComputeShaderUsage(
              AccessType::kWriteOnly),
          ImageUsage::GetLinearAccessInComputeShaderUsage(
              AccessType::kReadOnly),
          ImageUsage::GetSampledInFragmentShaderUsage(),
      });

  const base::Image::Dimension rgba_dim{paths_image_dimension,
                                        paths_image_dimension,
                                        base::image::kRgbaImageChannel};
  distance_field_image_ = renderer.CreateColorImage(
      "DistanceField", rgba_dim, MultisamplingMode::kNone,
      /*high_precision=*/true,
      std::array{
          ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0),
          ImageUsage::GetMultisampleResolveTargetUsage(),
          ImageUsage::GetLinearAccessInComputeShaderUsage(
              AccessType::kReadOnly),
          ImageUsage::GetLinearAccessInComputeShaderUsage(
              AccessType::kReadWrite),
          ImageUsage::GetSampledInFragmentShaderUsage(),
      });
}

void PathDumper::DumpAuroraPaths(Renderer& renderer,
                                 const scene::Camera& camera,
                                 std::span<const SplinePathView> spline_paths) {
#ifndef NDEBUG
  const base::BasicTimer timer;
#endif

  RenderGraph graph;
  const renderer::ir::ImageHandle paths_handle =
      graph.ImportImage("AuroraPaths", *paths_image_, ImageUsage{},
                        ImageUsage::GetSampledInFragmentShaderUsage());
  const renderer::ir::ImageHandle dist_handle =
      graph.ImportImage("DistanceField", *distance_field_image_, ImageUsage{},
                        ImageUsage::GetSampledInFragmentShaderUsage());

  const auto dumped = path_renderer_.AddPasses(
      graph, /*intermediate_image=*/dist_handle, /*output_image=*/paths_handle,
      camera, spline_paths);
  distance_field_generator_.AddPasses(graph, dumped.output,
                                      dumped.intermediate);
  renderer.ExecuteRenderGraph(graph);

#ifndef NDEBUG
  LOG_INFO << std::format("Elapsed time for dumping aurora paths: {}s",
                          timer.GetElapsedTimeSinceLaunch());
#endif
}

}  // namespace lighter::example::aurora
