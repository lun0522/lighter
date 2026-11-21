//
//  distance_field.cc
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/viewer/distance_field.h"

#include <algorithm>

#include "lighter/example/util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::example::aurora {
namespace {

using renderer::ir::AccessType;
using renderer::ir::ImageHandle;
using renderer::ir::ImageUsage;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::RenderGraph;
using renderer::ir::TransientImageDesc;
using renderer::ir::TransientImageFormat;

constexpr uint32_t kOriginalImageBindingPoint = 0;
constexpr uint32_t kOutputImageBindingPoint = 1;
constexpr uint32_t kWorkGroupSizeX = 16;
constexpr uint32_t kWorkGroupSizeY = 16;

struct StepWidth {
  ALIGN_SCALAR(int32_t) int32_t value;
};

uint32_t GetWorkGroupCount(int extent, uint32_t work_group_size) {
  return (static_cast<uint32_t>(extent) + work_group_size - 1) /
         work_group_size;
}

}  // namespace

DistanceFieldGenerator::DistanceFieldGenerator(const glm::ivec2& image_extent)
    : image_extent_{image_extent},
      group_count_x_{GetWorkGroupCount(image_extent.x, kWorkGroupSizeX)},
      group_count_y_{GetWorkGroupCount(image_extent.y, kWorkGroupSizeY)} {
  const int greatest_dimension = std::max(image_extent.x, image_extent.y);
  for (int dim = 1; dim < greatest_dimension; dim *= 2) {
    step_widths_.push_back(dim);
  }

  path_to_coord_pipeline_.SetName("PathToCoord")
      .SetShader(GetShaderBinaryPath("aurora/path_to_coord.comp"));

  jump_flooding_pipeline_.SetName("JumpFlooding")
      .SetShader(GetShaderBinaryPath("aurora/jump_flooding.comp"));

  coord_to_dist_pipeline_.SetName("CoordToDist")
      .SetShader(GetShaderBinaryPath("aurora/coord_to_dist.comp"));
}

ImageHandle DistanceFieldGenerator::AddPasses(RenderGraph& graph,
                                              ImageHandle input_image,
                                              ImageHandle ping_image) const {
  ImageHandle pong_image;
  graph.AddComputePass(
      "PathToCoord",
      [&](PassBuilder& builder) {
        input_image = builder.Read(
            input_image, ImageUsage::GetLinearAccessInComputeShaderUsage(
                             AccessType::kReadOnly));
        ping_image = builder.Write(
            ping_image, ImageUsage::GetLinearAccessInComputeShaderUsage(
                            AccessType::kWriteOnly));
        pong_image = builder.CreateImage(TransientImageDesc{
            .name = "jfa_pong",
            .extent = image_extent_,
            .format = TransientImageFormat::kColorFloat16,
        });
      },
      [this, input_image, ping_image](PassContext& ctx) {
        ctx.BindComputePipeline(path_to_coord_pipeline_);
        ctx.BindImage(kOriginalImageBindingPoint, input_image);
        ctx.BindImage(kOutputImageBindingPoint, ping_image);
        ctx.Dispatch(group_count_x_, group_count_y_);
      });

  bool write_to_pong = true;
  for (int width : step_widths_) {
    const StepWidth step{width};
    ImageHandle src = write_to_pong ? ping_image : pong_image;
    ImageHandle dst = write_to_pong ? pong_image : ping_image;
    graph.AddComputePass(
        "JumpFlooding",
        [&](PassBuilder& builder) {
          src =
              builder.Read(src, ImageUsage::GetLinearAccessInComputeShaderUsage(
                                    AccessType::kReadOnly));
          dst = builder.Write(dst,
                              ImageUsage::GetLinearAccessInComputeShaderUsage(
                                  AccessType::kWriteOnly));
        },
        [this, src, dst, step](PassContext& ctx) {
          ctx.BindComputePipeline(jump_flooding_pipeline_);
          ctx.PushConstants(renderer::ir::shader_stage::COMPUTE, /*offset=*/0,
                            sizeof(StepWidth), &step);
          ctx.BindImage(kOriginalImageBindingPoint, src);
          ctx.BindImage(kOutputImageBindingPoint, dst);
          ctx.Dispatch(group_count_x_, group_count_y_);
        });
    if (write_to_pong) {
      pong_image = dst;
    } else {
      ping_image = dst;
    }
    write_to_pong = !write_to_pong;
  }

  // Last pass reads whichever buffer currently holds coordinates (ping if
  // write_to_pong == true, else pong) and writes final distances into ping.
  const bool coord_in_ping = write_to_pong;
  if (coord_in_ping) {
    graph.AddComputePass(
        "CoordToDist",
        [&](PassBuilder& builder) {
          ping_image = builder.Write(
              ping_image, ImageUsage::GetLinearAccessInComputeShaderUsage(
                              AccessType::kReadWrite));
        },
        [this, ping_image](PassContext& ctx) {
          ctx.BindComputePipeline(coord_to_dist_pipeline_);
          ctx.BindImage(kOriginalImageBindingPoint, ping_image);
          ctx.BindImage(kOutputImageBindingPoint, ping_image);
          ctx.Dispatch(group_count_x_, group_count_y_);
        });
  } else {
    graph.AddComputePass(
        "CoordToDist",
        [&](PassBuilder& builder) {
          pong_image = builder.Read(
              pong_image, ImageUsage::GetLinearAccessInComputeShaderUsage(
                              AccessType::kReadOnly));
          ping_image = builder.Write(
              ping_image, ImageUsage::GetLinearAccessInComputeShaderUsage(
                              AccessType::kReadWrite));
        },
        [this, pong_image, ping_image](PassContext& ctx) {
          ctx.BindComputePipeline(coord_to_dist_pipeline_);
          ctx.BindImage(kOriginalImageBindingPoint, pong_image);
          ctx.BindImage(kOutputImageBindingPoint, ping_image);
          ctx.Dispatch(group_count_x_, group_count_y_);
        });
  }

  return ping_image;
}

}  // namespace lighter::example::aurora
