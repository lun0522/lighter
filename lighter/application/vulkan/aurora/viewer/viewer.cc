//
//  viewer.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/viewer/viewer.h"

#include <array>

#include "lighter/application/vulkan/aurora/viewer/air_transmit_table.h"
#include "lighter/application/vulkan/util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/util.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace renderer;
using namespace renderer::vulkan;

enum SubpassIndex {
  kViewImageSubpassIndex = 0,
  kNumSubpasses,
};

enum UniformBindingPoint {
  kCameraUniformBindingPoint = 0,
  kAuroraDepositionImageBindingPoint,
  kAuroraPathsImageBindingPoint,
  kDistanceFieldImageBindingPoint,
  kAirTransmitTableImageBindingPoint,
  kUniverseSkyboxImageBindingPoint,
  kNumUniformBindingPoints,
  kNumImages = kNumUniformBindingPoints - kAuroraDepositionImageBindingPoint,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct CameraParameter {
  ALIGN_VEC4 glm::vec4 up;
  ALIGN_VEC4 glm::vec4 front;
  ALIGN_VEC4 glm::vec4 right;
};

struct RenderInfo {
  ALIGN_VEC4 glm::vec4 camera_pos;
  ALIGN_MAT4 glm::mat4 aurora_proj_view;
};

/* END: Consistent with uniform blocks defined in shaders. */

// Returns the axis of earth model in object space.
const glm::vec3& GetEarthModelAxis() {
  static const auto* earth_model_axis = new glm::vec3{0.0f, 1.0f, 0.0f};
  return *earth_model_axis;
}

} /* namespace */

ViewerRenderer::ViewerRenderer(const WindowContext* window_context,
                               int num_frames_in_flight,
                               float air_transmit_sample_step,
                               const SamplableImage& aurora_paths_image,
                               const SamplableImage& distance_field_image)
    : window_context_{*FATAL_IF_NULL(window_context)} {
  using common::Vertex2DPosOnly;
  const auto context = window_context_.basic_context();

  /* Uniform buffer and push constant */
  camera_constant_ = std::make_unique<PushConstant>(
      context, sizeof(CameraParameter), num_frames_in_flight);
  render_info_uniform_ = std::make_unique<UniformBuffer>(
      context, sizeof(RenderInfo), num_frames_in_flight);

  /* Image */
  const auto image_usages = {ImageUsage::GetSampledInFragmentShaderUsage()};
  const ImageSampler::Config sampler_config{
      VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  aurora_deposition_image_ = std::make_unique<SharedTexture>(
      context, common::file::GetResourcePath("texture/aurora_deposition.jpg"),
      image_usages, sampler_config);

  const auto air_transmit_table =
      GenerateAirTransmitTable(air_transmit_sample_step);
  air_transmit_table_image_ = std::make_unique<TextureImage>(
      context, /*generate_mipmaps=*/false, air_transmit_table, image_usages,
      sampler_config);

  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/
      common::file::GetResourcePath("texture/universe/PositiveX.jpg",
                                    /*want_directory_path=*/true),
      /*files=*/{
          "PositiveX.jpg", "NegativeX.jpg",
          "PositiveY.jpg", "NegativeY.jpg",
          "PositiveZ.jpg", "NegativeZ.jpg",
      },
  };
  universe_skybox_image_ = std::make_unique<SharedTexture>(
      context, skybox_path, image_usages, ImageSampler::Config{});

  /* Descriptor */
  std::vector<Descriptor::Info::Binding> image_bindings;
  image_bindings.reserve(kNumImages);
  for (int i = kAuroraDepositionImageBindingPoint;
       i <= kUniverseSkyboxImageBindingPoint; ++i) {
    image_bindings.push_back(Descriptor::Info::Binding{
        /*binding_point=*/static_cast<uint32_t>(i),
        /*array_length=*/1,
    });
  }
  const std::array<Descriptor::Info, 2> descriptor_infos{
      Descriptor::Info{
          UniformBuffer::GetDescriptorType(),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          /*bindings=*/{{kCameraUniformBindingPoint, /*array_length=*/1}},
      },
      Descriptor::Info{
          Image::GetDescriptorTypeForSampling(),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          image_bindings,
      },
  };

  const Descriptor::ImageInfoMap image_info_map{
      {kAuroraDepositionImageBindingPoint,
          {aurora_deposition_image_->GetDescriptorInfoForSampling()}},
      {kAuroraPathsImageBindingPoint,
          {aurora_paths_image.GetDescriptorInfoForSampling()}},
      {kDistanceFieldImageBindingPoint,
          {distance_field_image.GetDescriptorInfoForSampling()}},
      {kAirTransmitTableImageBindingPoint,
          {air_transmit_table_image_->GetDescriptorInfoForSampling()}},
      {kUniverseSkyboxImageBindingPoint,
          {universe_skybox_image_->GetDescriptorInfoForSampling()}}
  };

  descriptors_.reserve(num_frames_in_flight);
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_.push_back(
        std::make_unique<StaticDescriptor>(context, descriptor_infos));
    (*descriptors_[frame])
        .UpdateBufferInfos(
            UniformBuffer::GetDescriptorType(),
            /*buffer_info_map=*/{
                {kCameraUniformBindingPoint,
                    {render_info_uniform_->GetDescriptorInfo(frame)}}})
        .UpdateImageInfos(Image::GetDescriptorTypeForSampling(),
                          image_info_map);
  }

  /* Vertex buffer */
  const auto vertex_data = Vertex2DPosOnly::GetFullScreenSquadVertices();
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = std::make_unique<StaticPerVertexBuffer>(
      context, vertex_data_info,
      pipeline::GetVertexAttributes<Vertex2DPosOnly>());

  /* Pipeline */
  pipeline_builder_ = std::make_unique<GraphicsPipelineBuilder>(context);
  (*pipeline_builder_)
      .SetPipelineName("View aurora")
      .AddVertexInput(
          kVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<Vertex2DPosOnly>(),
          vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptors_[0]->layout()},
                         {camera_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("aurora/aurora.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("aurora/aurora.frag"));

  /* Render pass */
  ImageUsageHistory usage_history{
    window_context_.swapchain_image(/*index=*/0).GetInitialUsage()};
  usage_history
      .AddUsage(kViewImageSubpassIndex,
                ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0))
      .SetFinalUsage(ImageUsage::GetPresentationUsage());

  GraphicsPass graphics_pass{context, kNumSubpasses};
  graphics_pass.AddAttachment("Swapchain", std::move(usage_history),
                              /*get_location=*/[](int subpass) { return 0; });
  render_pass_builder_ = graphics_pass.CreateRenderPassBuilder(
      /*num_framebuffers=*/window_context_.num_swapchain_images());
}

void ViewerRenderer::Recreate() {
  render_pass_builder_->UpdateAttachmentImage(
      /*index=*/0,
      [this](int framebuffer_index) -> const Image& {
        return window_context_.swapchain_image(framebuffer_index);
      });
  render_pass_ = render_pass_builder_->Build();

  (*pipeline_builder_)
      .SetViewport(pipeline::GetViewport(
          window_context_.frame_size(),
          window_context_.original_aspect_ratio()))
      .SetRenderPass(**render_pass_, kViewImageSubpassIndex);
  pipeline_ = pipeline_builder_->Build();
}

void ViewerRenderer::UpdateDumpPathsCamera(const common::Camera& camera) {
  const glm::mat4 proj_view = camera.GetProjectionMatrix() *
                              camera.GetViewMatrix();
  for (int frame = 0; frame < descriptors_.size(); ++frame) {
    render_info_uniform_->HostData<RenderInfo>(frame)->aurora_proj_view =
        proj_view;
    render_info_uniform_->Flush(frame, sizeof(RenderInfo::aurora_proj_view),
                                offsetof(RenderInfo, aurora_proj_view));
  }
}

void ViewerRenderer::UpdateViewAuroraCamera(
    int frame, const common::PerspectiveCamera& camera) {
  render_info_uniform_->HostData<RenderInfo>(frame)->camera_pos =
      glm::vec4{camera.position(), 0.0f};
  render_info_uniform_->Flush(frame, sizeof(RenderInfo::camera_pos),
                              offsetof(RenderInfo, camera_pos));

  const auto ray_tracing_params = camera.GetRayTracingParams();
  auto& camera_parameter = *camera_constant_->HostData<CameraParameter>(frame);
  camera_parameter.up = glm::vec4{ray_tracing_params.up, 0.0f};
  camera_parameter.front = glm::vec4{ray_tracing_params.front, 0.0f};
  camera_parameter.right = glm::vec4{ray_tracing_params.right, 0.0f};
}

void ViewerRenderer::Draw(const VkCommandBuffer& command_buffer,
                          uint32_t framebuffer_index, int current_frame) const {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        pipeline_->Bind(command_buffer);
        descriptors_[current_frame]->Bind(command_buffer, pipeline_->layout(),
                                          pipeline_->binding_point());
        camera_constant_->Flush(command_buffer, pipeline_->layout(),
                                current_frame, /*target_offset=*/0,
                                VK_SHADER_STAGE_VERTEX_BIT);
        vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                             /*mesh_index=*/0, /*instance_count=*/1);
      },
  });
}

Viewer::Viewer(
    WindowContext* window_context, int num_frames_in_flight,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : window_context_{*FATAL_IF_NULL(window_context)},
      path_dumper_{window_context_.basic_context(),
                   /*paths_image_dimension=*/1024,
                   std::move(aurora_paths_vertex_buffers)},
      viewer_renderer_{window_context, num_frames_in_flight,
                       /*air_transmit_sample_step=*/0.01f,
                       path_dumper_.aurora_paths_image(),
                       path_dumper_.distance_field_image()} {
  common::Camera::Config camera_config;
  camera_config.far = 2.0f;
  camera_config.up = GetEarthModelAxis();
  camera_config.position = glm::vec3{0.0f};
  // The 'look_at' point doesn't matter at this moment. It just can't be the
  // position of camera itself. It will be updated to user viewpoint.
  camera_config.look_at = glm::vec3{1.0f};
  // Field of view should be as small as possible so that we can focus on more
  // details of aurora paths, but it should not be too small, in case that the
  // marching ray goes out of the resulting texture.
  dump_paths_camera_ = std::make_unique<common::PerspectiveCamera>(
      camera_config, common::PerspectiveCamera::FrustumConfig{
          /*field_of_view_y=*/40.0f, /*aspect_ratio=*/1.0f,
      });

  // 'position' and 'look_at' don't matter at this moment. They will be set
  // according to the user viewpoint.
  view_aurora_camera_ = common::UserControlledPerspectiveCamera::Create(
      /*control_config=*/{}, camera_config,
      common::PerspectiveCamera::FrustumConfig{
          /*field_of_view_y=*/45.0f, window_context_.original_aspect_ratio(),
      });
  view_aurora_camera_->SetActivity(true);
}

void Viewer::UpdateAuroraPaths(const glm::vec3& viewpoint_position) {
  dump_paths_camera_->SetFront(viewpoint_position);
  path_dumper_.DumpAuroraPaths(*dump_paths_camera_);
  viewer_renderer_.UpdateDumpPathsCamera(*dump_paths_camera_);

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
  (*window_context_.mutable_window())
      .SetCursorHidden(true)
      .RegisterMoveCursorCallback([this](double x_pos, double y_pos) {
        view_aurora_camera_->DidMoveCursor(x_pos, y_pos);
      })
      .RegisterScrollCallback([this](double x_pos, double y_pos) {
        view_aurora_camera_->DidScroll(y_pos, 15.0f, 45.0f);
      })
      .RegisterPressKeyCallback(common::Window::KeyMap::kEscape,
                                [this]() { should_quit_ = true; });
}

void Viewer::OnExit() {
  (*window_context_.mutable_window())
      .SetCursorHidden(false)
      .RegisterMoveCursorCallback(nullptr)
      .RegisterScrollCallback(nullptr)
      .RegisterPressKeyCallback(common::Window::KeyMap::kEscape, nullptr);
}

void Viewer::Recreate() {
  view_aurora_camera_->SetCursorPos(window_context_.window().GetCursorPos());
  viewer_renderer_.Recreate();
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
