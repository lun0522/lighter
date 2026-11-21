//
//  viewer.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/viewer/viewer.h"

#include <array>
#include <cstddef>

#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/example/aurora/viewer/air_transmit_table.h"
#include "lighter/example/util.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/buffer_util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/pipeline_util.h"

namespace lighter::example::aurora {
namespace {

using common::Vertex2DPosOnly;
using renderer::ir::AccessLocation;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::FilterType;
using renderer::ir::GraphicsPipelineDescriptor;
using renderer::ir::ImageHandle;
using renderer::ir::ImageUsage;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;
using renderer::ir::SamplerAddressMode;
using renderer::ir::VertexInputRate;

enum UniformBindingPoint {
  kCameraUniformBindingPoint = 0,
  kAuroraDepositionImageBindingPoint,
  kAuroraPathsImageBindingPoint,
  kDistanceFieldImageBindingPoint,
  kAirTransmitTableImageBindingPoint,
  kUniverseSkyboxImageBindingPoint,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;
constexpr float kAirTransmitSampleStep = 0.01f;
constexpr int kPathsImageDimension = 1024;

// Consistent with uniform blocks defined in shaders.
struct CameraParameter {
  ALIGN_VEC4 glm::vec4 up;
  ALIGN_VEC4 glm::vec4 front;
  ALIGN_VEC4 glm::vec4 right;
};

struct RenderInfo {
  ALIGN_VEC4 glm::vec4 camera_pos;
  ALIGN_MAT4 glm::mat4 aurora_proj_view;
};

const glm::vec3& GetEarthModelAxis() {
  static const auto* earth_model_axis = new glm::vec3{0.0f, 1.0f, 0.0f};
  return *earth_model_axis;
}

}  // namespace

Viewer::Viewer(common::Window& window, Renderer& renderer, int window_index,
               MultisamplingMode multisampling_mode, int num_frames_in_flight)
    : window_{window},
      renderer_{renderer},
      window_index_{window_index},
      multisampling_mode_{multisampling_mode},
      path_dumper_{renderer_, kPathsImageDimension} {
  using common::file::GetResourcePath;

  const std::array uniform_usages{
      BufferUsage::GetUniformBufferUsage(AccessLocation::kFragmentShader)};
  render_info_uniforms_.resize(static_cast<size_t>(num_frames_in_flight));
  for (auto& buffer : render_info_uniforms_) {
    buffer = renderer_.CreateBuffer(Buffer::UpdateRate::kHigh,
                                    sizeof(RenderInfo), uniform_usages);
  }

  const std::array sampled_usage{ImageUsage::GetSampledInFragmentShaderUsage()};
  clamp_sampler_ = {
      .filter_type = FilterType::kLinear,
      .address_mode = SamplerAddressMode::kClampToEdge,
  };
  repeat_sampler_ = {
      .filter_type = FilterType::kLinear,
      .address_mode = SamplerAddressMode::kRepeat,
  };

  aurora_deposition_image_ = renderer_.CreateColorImage(
      "AuroraDeposition",
      common::Image::LoadSingleImageFromFile(
          GetResourcePath("texture/aurora_deposition.jpg"), /*flip_y=*/false),
      /*generate_mipmaps=*/true, sampled_usage);

  air_transmit_table_image_ = renderer_.CreateColorImage(
      "AirTransmitTable", GenerateAirTransmitTable(kAirTransmitSampleStep),
      /*generate_mipmaps=*/false, sampled_usage);

  const std::array<std::string, 6> skybox_files{
      "PositiveX.jpg", "NegativeX.jpg", "PositiveY.jpg",
      "NegativeY.jpg", "PositiveZ.jpg", "NegativeZ.jpg",
  };
  universe_skybox_image_ = renderer_.CreateColorImage(
      "UniverseSkybox",
      common::Image::LoadCubemapFromFiles(
          GetResourcePath("texture/universe/PositiveX.jpg",
                          /*want_directory_path=*/true),
          skybox_files, /*flip_y=*/false),
      /*generate_mipmaps=*/false, sampled_usage);

  const std::array<Vertex2DPosOnly, 6> quad_vertices =
      Vertex2DPosOnly::GetFullScreenSquadVertices();
  const size_t quad_bytes = common::util::GetTotalDataSize(
      std::span<const Vertex2DPosOnly>{quad_vertices});
  quad_vertex_buffer_ =
      renderer_.CreateBuffer(Buffer::UpdateRate::kLow, quad_bytes,
                             std::array{BufferUsage::GetVertexBufferUsage(
                                 BufferUsage::UsageType::kVertexOnly)});
  quad_vertex_buffer_->CopyToDevice(std::array{
      Buffer::CopyInfo{quad_vertices.data(), quad_bytes, /*offset=*/0}});

  base_pipeline_descriptor_.SetName("ViewAurora")
      .SetShader(renderer::ir::shader_stage::VERTEX,
                 GetShaderBinaryPath("aurora/aurora.vert"))
      .SetShader(renderer::ir::shader_stage::FRAGMENT,
                 GetShaderBinaryPath("aurora/aurora.frag"))
      .UseColorAttachment(/*location=*/0, std::nullopt)
      .AddVertexInput({
          VertexInputRate::kVertex,
          kVertexBufferBindingPoint,
          sizeof(Vertex2DPosOnly),
          renderer::ir::buffer::CreateAttributesForVertex2DPosOnly(0),
      });

  common::Camera::Config camera_config;
  camera_config.far = 2.0f;
  camera_config.up = GetEarthModelAxis();
  camera_config.position = glm::vec3{0.0f};
  camera_config.look_at = glm::vec3{1.0f};
  dump_paths_camera_ = std::make_unique<common::PerspectiveCamera>(
      camera_config, common::PerspectiveCamera::FrustumConfig{
                         /*field_of_view_y=*/40.0f,
                         /*aspect_ratio=*/1.0f,
                     });

  view_aurora_camera_ = common::UserControlledPerspectiveCamera::Create(
      /*control_config=*/{}, camera_config,
      common::PerspectiveCamera::FrustumConfig{
          /*field_of_view_y=*/45.0f,
          window_.original_aspect_ratio(),
      });
  view_aurora_camera_->SetActivity(true);
}

void Viewer::UpdateAuroraPaths(const glm::vec3& viewpoint_position,
                               std::span<const SplinePathView> spline_paths) {
  dump_paths_camera_->SetFront(viewpoint_position);
  path_dumper_.DumpAuroraPaths(renderer_, *dump_paths_camera_, spline_paths);
  aurora_proj_view_ = dump_paths_camera_->GetProjectionMatrix() *
                      dump_paths_camera_->GetViewMatrix();

  view_aurora_camera_->SetInternalStates(
      [&viewpoint_position](common::PerspectiveCamera* camera) {
        camera->SetPosition(viewpoint_position);
        camera->SetUp(viewpoint_position);
        const glm::vec3 right = glm::cross(GetEarthModelAxis(), camera->up());
        camera->SetFront(glm::normalize(glm::cross(camera->up(), right)));
      });
}

void Viewer::OnEnter() {
  should_quit_ = false;
  window_.SetCursorHidden(true)
      .RegisterMoveCursorCallback([this](double x_pos, double y_pos) {
        view_aurora_camera_->DidMoveCursor(x_pos, y_pos);
      })
      .RegisterScrollCallback([this](double /*x_pos*/, double y_pos) {
        view_aurora_camera_->DidScroll(y_pos, 15.0f, 45.0f);
      })
      .RegisterPressKeyCallback(common::Window::KeyMap::kEscape,
                                [this]() { should_quit_ = true; });
  OnSurfaceRecreated();
}

void Viewer::OnExit() {
  window_.SetCursorHidden(false)
      .RegisterMoveCursorCallback(nullptr)
      .RegisterScrollCallback(nullptr)
      .RegisterPressKeyCallback(common::Window::KeyMap::kEscape, nullptr);
}

void Viewer::OnSurfaceRecreated() {
  view_aurora_camera_->SetCursorPos(window_.GetCursorPos());
}

void Viewer::BuildFrame(RenderGraph& graph, int frame_slot) {
  const common::PerspectiveCamera& cam = view_aurora_camera_->camera();
  const RenderInfo render_info{
      .camera_pos = glm::vec4{cam.position(), 0.0f},
      .aurora_proj_view = aurora_proj_view_,
  };
  const auto ray_tracing = cam.GetRayTracingParams();
  const CameraParameter camera_param{
      .up = glm::vec4{ray_tracing.up, 0.0f},
      .front = glm::vec4{ray_tracing.front, 0.0f},
      .right = glm::vec4{ray_tracing.right, 0.0f},
  };

  const ImageHandle backbuffer = graph.ImportSwapchainImage(window_index_);
  graph.AddGraphicsPass(
      "AuroraViewer",
      [&](PassBuilder& builder) {
        SetupMainPassAttachments(builder, backbuffer, multisampling_mode_,
                                 /*use_depth=*/false);
      },
      [this, backbuffer, frame_slot, render_info,
       camera_param](PassContext& ctx) {
        const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
        const Buffer& uniform =
            *render_info_uniforms_[static_cast<size_t>(frame_slot)];
        uniform.CopyToDevice(
            std::array{Buffer::CopyInfo{&render_info, sizeof(RenderInfo), 0}});

        GraphicsPipelineDescriptor desc = base_pipeline_descriptor_;
        desc.SetViewport(
            renderer::ir::pipeline::GetViewport(
                frame_size, window_.original_aspect_ratio()),
            renderer::ir::pipeline::GetFullFrameScissor(frame_size),
            /*flip_y=*/true);

        ctx.BindGraphicsPipeline(desc);
        ctx.BindBuffer(kCameraUniformBindingPoint, uniform);
        ctx.BindExternalImage(kAuroraDepositionImageBindingPoint,
                              *aurora_deposition_image_, &clamp_sampler_);
        ctx.BindExternalImage(kAuroraPathsImageBindingPoint,
                              path_dumper_.aurora_paths_image(),
                              &clamp_sampler_);
        ctx.BindExternalImage(kDistanceFieldImageBindingPoint,
                              path_dumper_.distance_field_image(),
                              &clamp_sampler_);
        ctx.BindExternalImage(kAirTransmitTableImageBindingPoint,
                              *air_transmit_table_image_, &clamp_sampler_);
        ctx.BindExternalImage(kUniverseSkyboxImageBindingPoint,
                              *universe_skybox_image_, &repeat_sampler_);
        ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                          sizeof(CameraParameter), &camera_param);
        ctx.BindVertexBuffer(kVertexBufferBindingPoint, *quad_vertex_buffer_);
        ctx.Draw(/*vertex_count=*/6);
      });
}

}  // namespace lighter::example::aurora
