//
//  viewer.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/viewer.h"

#include "jessie_steamer/application/vulkan/aurora/viewer/air_transmit_table.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kViewImageSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kViewImageSubpassIndex,
};

enum UniformBindingPoint {
  kCameraUniformBindingPoint = 0,
  kAuroraDepositionImageBindingPoint,
  kAuroraPathsImageBindingPoint,
  kDistanceFieldImageBindingPoint,
  kAirTransmitTableImageBindingPoint,
  kUniverseSkyboxImageBindingPoint,
  kNumUniformBindingPoints,
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
  static const glm::vec3* earth_model_axis = nullptr;
  if (earth_model_axis == nullptr) {
    earth_model_axis = new glm::vec3{0.0f, 1.0f, 0.0f};
  }
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
  const auto& context = window_context_.basic_context();

  /* Uniform buffer and push constant */
  camera_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(CameraParameter), num_frames_in_flight);
  render_info_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(RenderInfo), num_frames_in_flight);

  /* Image */
  const auto image_usage_flags = image::GetImageUsageFlags(
      {image::Usage::kSampledInFragmentShader});
  const ImageSampler::Config sampler_config{
      VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  aurora_deposition_image_ = absl::make_unique<SharedTexture>(
      context, common::file::GetResourcePath("texture/aurora_deposition.jpg"),
      image_usage_flags, sampler_config);

  const auto air_transmit_table =
      GenerateAirTransmitTable(air_transmit_sample_step);
  air_transmit_table_image_ = absl::make_unique<TextureImage>(
      context, /*generate_mipmaps=*/false, image_usage_flags,
      *air_transmit_table, sampler_config);

  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/common::file::GetResourcePath("texture/universe"),
      /*files=*/{
          "PositiveX.jpg", "NegativeX.jpg",
          "PositiveY.jpg", "NegativeY.jpg",
          "PositiveZ.jpg", "NegativeZ.jpg",
      },
  };
  universe_skybox_image_ = absl::make_unique<SharedTexture>(
      context, skybox_path, image_usage_flags, ImageSampler::Config{});

  /* Descriptor */
  const Descriptor::ImageInfoMap image_info_map{
      {kAuroraDepositionImageBindingPoint,
          {aurora_deposition_image_->GetDescriptorInfo()}},
      {kAuroraPathsImageBindingPoint,
          {aurora_paths_image.GetDescriptorInfo()}},
      {kDistanceFieldImageBindingPoint,
          {distance_field_image.GetDescriptorInfo()}},
      {kAirTransmitTableImageBindingPoint,
          {air_transmit_table_image_->GetDescriptorInfo()}},
      {kUniverseSkyboxImageBindingPoint,
          {universe_skybox_image_->GetDescriptorInfo()}}
  };

  std::vector<Descriptor::Info> uniform_descriptor_infos;
  uniform_descriptor_infos.reserve(kNumUniformBindingPoints);
  uniform_descriptor_infos.emplace_back(Descriptor::Info{
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      /*bindings=*/{
          Descriptor::Info::Binding{
              kCameraUniformBindingPoint,
              /*array_length=*/1,
          }},
  });
  for (int i = kCameraUniformBindingPoint + 1; i < kNumUniformBindingPoints;
       ++i) {
    uniform_descriptor_infos.emplace_back(Descriptor::Info{
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        /*bindings=*/{
            Descriptor::Info::Binding{
                /*binding_point=*/static_cast<uint32_t>(i),
                /*array_length=*/1,
            }},
    });
  }

  descriptors_.reserve(num_frames_in_flight);
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_.emplace_back(absl::make_unique<StaticDescriptor>(
        context, uniform_descriptor_infos));
    (*descriptors_[frame])
        .UpdateBufferInfos(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            /*buffer_info_map=*/{
                {kCameraUniformBindingPoint,
                    {render_info_uniform_->GetDescriptorInfo(frame)}}})
        .UpdateImageInfos(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          image_info_map);
  }

  /* Vertex buffer */
  const auto vertex_data = Vertex2DPosOnly::GetFullScreenSquadVertices();
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context, vertex_data_info,
      pipeline::GetVertexAttribute<Vertex2DPosOnly>());

  /* Pipeline */
  pipeline_builder_ = absl::make_unique<GraphicsPipelineBuilder>(context);
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
                 common::file::GetVkShaderPath("aurora/aurora.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("aurora/aurora.frag"));

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      window_context_.basic_context(), subpass_config,
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      /*use_multisampling=*/false,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kPresentToScreen);
}

void ViewerRenderer::Recreate() {
  render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder_->color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context_.swapchain_image(framebuffer_index);
      });
  render_pass_ = (*render_pass_builder_)->Build();

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
    int frame, const common::Camera& camera, float view_aurora_camera_fovy) {
  render_info_uniform_->HostData<RenderInfo>(frame)->camera_pos =
      glm::vec4{camera.position(), 0.0f};
  render_info_uniform_->Flush(frame, sizeof(RenderInfo::camera_pos),
                              offsetof(RenderInfo, camera_pos));

  const glm::vec3 up_dir =
      glm::normalize(glm::cross(camera.right(), camera.front()));
  const float tan_fovy = glm::tan(glm::radians(view_aurora_camera_fovy));
  auto& camera_parameter = *camera_constant_->HostData<CameraParameter>(frame);
  camera_parameter.up = glm::vec4{up_dir * tan_fovy, 0.0f};
  camera_parameter.front = glm::vec4{camera.front(), 0.0f};
  camera_parameter.right = glm::vec4{camera.right() * tan_fovy *
                                     window_context_.original_aspect_ratio(),
                                     0.0f};
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
  common::Camera::Config config;
  config.far = 2.0f;
  config.up = GetEarthModelAxis();
  config.position = glm::vec3{0.0f};
  // The 'look_at' point doesn't matter at this moment. It just can't be the
  // position of camera itself. It will be updated to user viewpoint.
  config.look_at = glm::vec3{1.0f};
  // Field of view should be as small as possible so that we can focus on more
  // details of aurora paths, but it should not be too small, in case that the
  // marching ray goes out of the resulting texture.
  const common::PerspectiveCamera::PersConfig pers_config{
      /*field_of_view_y=*/40.0f, /*aspect_ratio=*/1.0f};
  dump_paths_camera_ =
      absl::make_unique<common::PerspectiveCamera>(config, pers_config);

  // Since we are using a fullscreen squad to do ray-tracing, we can simply use
  // an orthographic camera. 'position' and 'look_at' don't matter at this
  // moment. They will be set according to the user viewpoint.
  auto ortho_camera = absl::make_unique<common::OrthographicCamera>(
      config, common::OrthographicCamera::GetFullscreenConfig());
  view_aurora_camera_ = absl::make_unique<common::UserControlledCamera>(
      common::UserControlledCamera::ControlConfig{}, std::move(ortho_camera));
  view_aurora_camera_->SetActivity(true);
}

void Viewer::UpdateAuroraPaths(const glm::vec3& viewpoint_position) {
  dump_paths_camera_->SetFront(viewpoint_position);
  path_dumper_.DumpAuroraPaths(*dump_paths_camera_);
  viewer_renderer_.UpdateDumpPathsCamera(*dump_paths_camera_);

  view_aurora_camera_->SetInternalStates(
      [&viewpoint_position](common::Camera* camera) {
        camera->SetPosition(viewpoint_position);
        camera->SetUp(viewpoint_position);
        // TODO: Should not need negation.
        camera->SetRight(-glm::cross(GetEarthModelAxis(), camera->up()));
      });
}

void Viewer::OnEnter() {
  // TODO: Find a better way to exit viewer.
  did_press_right_ = false;
  (*window_context_.mutable_window())
      .SetCursorHidden(true)
      .RegisterMoveCursorCallback([this](double x_pos, double y_pos) {
        view_aurora_camera_->DidMoveCursor(x_pos, y_pos);
      })
      .RegisterScrollCallback([this](double x_pos, double y_pos) {
        view_aurora_camera_fovy_ =
            glm::clamp(view_aurora_camera_fovy_ + static_cast<float>(y_pos),
                       15.0f, 60.0f);
      })
      .RegisterMouseButtonCallback([this](bool is_left, bool is_press) {
        did_press_right_ = !is_left && is_press;
      });
}

void Viewer::OnExit() {
  (*window_context_.mutable_window())
      .SetCursorHidden(false)
      .RegisterMoveCursorCallback(nullptr)
      .RegisterMouseButtonCallback(nullptr);
}

void Viewer::Recreate() {
  view_aurora_camera_->SetCursorPos(window_context_.window().GetCursorPos());
  viewer_renderer_.Recreate();
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
