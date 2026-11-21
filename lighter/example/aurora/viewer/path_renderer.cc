//
//  path_renderer.cc
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/viewer/path_renderer.h"

#include <optional>
#include <vector>

#include "lighter/example/util.h"
#include "lighter/renderer/ir/buffer_util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/pipeline_util.h"

namespace lighter::example::aurora {
namespace {

using renderer::ir::AccessType;
using renderer::ir::AttachmentLoadOp;
using renderer::ir::AttachmentStoreOp;
using renderer::ir::ImageHandle;
using renderer::ir::ImageUsage;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::PrimitiveTopology;
using renderer::ir::RenderGraph;
using renderer::ir::TransientImageDesc;
using renderer::ir::TransientImageFormat;
using renderer::ir::VertexInputRate;

constexpr uint32_t kVertexBufferBindingPoint = 0;
constexpr uint32_t kOriginalImageBindingPoint = 0;
constexpr uint32_t kOutputImageBindingPoint = 1;
constexpr uint32_t kWorkGroupSizeX = 16;
constexpr uint32_t kWorkGroupSizeY = 16;

struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view;
};

uint32_t GetWorkGroupCount(int extent, uint32_t work_group_size) {
  return (static_cast<uint32_t>(extent) + work_group_size - 1) /
         work_group_size;
}

}  // namespace

PathRenderer2D::PathRenderer2D(const glm::ivec2& image_extent,
                               MultisamplingMode multisampling_mode)
    : image_extent_{image_extent},
      multisampling_mode_{multisampling_mode},
      group_count_x_{GetWorkGroupCount(image_extent.x, kWorkGroupSizeX)},
      group_count_y_{GetWorkGroupCount(image_extent.y, kWorkGroupSizeY)} {
  render_paths_pipeline_.SetName("DumpPath")
      .SetShader(renderer::ir::shader_stage::VERTEX,
                 GetShaderBinaryPath("aurora/dump_path.vert"))
      .SetShader(renderer::ir::shader_stage::FRAGMENT,
                 GetShaderBinaryPath("aurora/dump_path.frag"))
      .SetPrimitiveTopology(PrimitiveTopology::kLineStrip)
      .UseColorAttachment(/*location=*/0, std::nullopt)
      .SetViewport(renderer::ir::pipeline::GetFullFrameViewport(image_extent_),
                   renderer::ir::pipeline::GetFullFrameScissor(image_extent_),
                   /*flip_y=*/false)
      .AddVertexInput({
          VertexInputRate::kVertex,
          kVertexBufferBindingPoint,
          sizeof(glm::vec3),
          renderer::ir::buffer::CreateAttributesForVertex3DPosOnly(0),
      });

  bold_paths_pipeline_.SetName("BoldPaths")
      .SetShader(GetShaderBinaryPath("aurora/bold_path.comp"));
}

PathRenderer2D::PassOutputs PathRenderer2D::AddPasses(
    RenderGraph& graph, ImageHandle intermediate_image,
    ImageHandle output_image, const common::Camera& camera,
    std::span<const SplinePathView> spline_paths) const {
  const Transformation trans{camera.GetProjectionMatrix() *
                             camera.GetViewMatrix()};
  const std::vector<SplinePathView> paths{spline_paths.begin(),
                                          spline_paths.end()};

  graph.AddGraphicsPass(
      "DumpAuroraPaths",
      [&](PassBuilder& builder) {
        if (multisampling_mode_ != MultisamplingMode::kNone) {
          ImageHandle msaa = builder.CreateImage(TransientImageDesc{
              .name = "dump_paths_msaa",
              .extent = image_extent_,
              .format = TransientImageFormat::kColorFloat16,
              .multisampling_mode = multisampling_mode_,
          });
          msaa = builder.ColorAttachment(msaa, /*location=*/0,
                                         AttachmentLoadOp::kClear,
                                         AttachmentStoreOp::kDontCare);
          intermediate_image =
              builder.ResolveAttachment(msaa, intermediate_image);
        } else {
          intermediate_image =
              builder.ColorAttachment(intermediate_image, /*location=*/0);
        }
      },
      [this, trans, paths](PassContext& ctx) {
        ctx.BindGraphicsPipeline(render_paths_pipeline_);
        ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                          sizeof(Transformation), &trans);
        for (const auto& path : paths) {
          if (path.vertex_count == 0) {
            continue;
          }
          ctx.BindVertexBuffer(kVertexBufferBindingPoint, *path.vertex_buffer);
          ctx.Draw(path.vertex_count);
        }
      });

  graph.AddComputePass(
      "BoldAuroraPaths",
      [&](PassBuilder& builder) {
        intermediate_image = builder.Read(
            intermediate_image, ImageUsage::GetLinearAccessInComputeShaderUsage(
                                    AccessType::kReadOnly));
        output_image = builder.Write(
            output_image, ImageUsage::GetLinearAccessInComputeShaderUsage(
                              AccessType::kWriteOnly));
      },
      [this, intermediate_image, output_image](PassContext& ctx) {
        ctx.BindComputePipeline(bold_paths_pipeline_);
        ctx.BindImage(kOriginalImageBindingPoint, intermediate_image);
        ctx.BindImage(kOutputImageBindingPoint, output_image);
        ctx.Dispatch(group_count_x_, group_count_y_);
      });

  return {.intermediate = intermediate_image, .output = output_image};
}

}  // namespace lighter::example::aurora
