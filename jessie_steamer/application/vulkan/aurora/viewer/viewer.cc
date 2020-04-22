//
//  viewer.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/viewer.h"

#include "jessie_steamer/common/util.h"

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
};

constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct CameraParameter {
  ALIGN_MAT4 glm::mat4 aurora_proj_view;
  ALIGN_VEC4 glm::vec4 pos;
  ALIGN_VEC4 glm::vec4 up;
  ALIGN_VEC4 glm::vec4 front;
  ALIGN_VEC4 glm::vec4 right;
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
                               const SamplableImage& aurora_paths_image,
                               const SamplableImage& distance_field_image)
    : window_context_{*FATAL_IF_NULL(window_context)} {
  using common::Vertex2DPosOnly;
  const auto& context = window_context_.basic_context();

  /* Uniform buffer */
  camera_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(CameraParameter), num_frames_in_flight);

  /* Image */
  aurora_deposition_image_ = absl::make_unique<SharedTexture>(
      context, common::file::GetResourcePath("texture/aurora.jpg"),
      image::GetImageUsageFlags({image::Usage::kSampledInFragmentShader}),
      ImageSampler::Config{VK_FILTER_LINEAR,
                           VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE});

  /* Descriptor */
  const Descriptor::ImageInfoMap image_info_map{
      {kAuroraDepositionImageBindingPoint,
          {aurora_deposition_image_->GetDescriptorInfo()}},
      {kAuroraPathsImageBindingPoint,
          {aurora_paths_image.GetDescriptorInfo()}},
      {kDistanceFieldImageBindingPoint,
          {distance_field_image.GetDescriptorInfo()}},
  };

  descriptors_.reserve(num_frames_in_flight);
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_.emplace_back(absl::make_unique<StaticDescriptor>(
        context, /*infos=*/std::vector<Descriptor::Info>{
            Descriptor::Info{
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                /*bindings=*/{
                    Descriptor::Info::Binding{
                        kCameraUniformBindingPoint,
                        /*array_length=*/1,
                    }},
            },
            Descriptor::Info{
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                /*bindings=*/{
                    Descriptor::Info::Binding{
                        kAuroraDepositionImageBindingPoint,
                        /*array_length=*/1,
                    }},
            },
            Descriptor::Info{
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                /*bindings=*/{
                    Descriptor::Info::Binding{
                        kAuroraPathsImageBindingPoint,
                        /*array_length=*/1,
                    }},
            },
            Descriptor::Info{
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                /*bindings=*/{
                    Descriptor::Info::Binding{
                        kDistanceFieldImageBindingPoint,
                        /*array_length=*/1,
                    }},
            },
        }));

    (*descriptors_[frame])
        .UpdateBufferInfos(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            /*buffer_info_map=*/{
                {kCameraUniformBindingPoint,
                    {camera_uniform_->GetDescriptorInfo(frame)}}})
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
                         /*push_constant_ranges=*/{})
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
    camera_uniform_->HostData<CameraParameter>(frame)->aurora_proj_view =
        proj_view;
    camera_uniform_->Flush(
        frame, /*data_size=*/sizeof(CameraParameter::aurora_proj_view),
        /*offset=*/0);
  }
}

void ViewerRenderer::UpdateViewAuroraCamera(
    int frame, const common::Camera& camera) {
  auto& camera_parameter = *camera_uniform_->HostData<CameraParameter>(frame);
  camera_parameter.pos = glm::vec4{camera.position(), 0.0f};
  camera_parameter.up = glm::vec4{glm::normalize(glm::cross(camera.right(),
                                                            camera.front())),
                                  0.0f};
  camera_parameter.front = glm::vec4{camera.front(), 0.0f};
  camera_parameter.right = glm::vec4{camera.right(), 0.0f};

  // Adjust 'up' or 'right' so that one of them will have length 1, while the
  // other one has length less than 1 because of the aspect ratio of screen.
  const float aspect_ratio = window_context_.original_aspect_ratio();
  if (aspect_ratio > 1.0f) {
    camera_parameter.up /= aspect_ratio;
  } else {
    camera_parameter.right * aspect_ratio;
  }

  const auto already_flushed_size = sizeof(CameraParameter::aurora_proj_view);
  camera_uniform_->Flush(
      frame, /*data_size=*/sizeof(CameraParameter) - already_flushed_size,
      /*offset=*/already_flushed_size);
}

void ViewerRenderer::Draw(const VkCommandBuffer& command_buffer,
                          uint32_t framebuffer_index, int current_frame) const {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        pipeline_->Bind(command_buffer);
        descriptors_[current_frame]->Bind(command_buffer, pipeline_->layout(),
                                          pipeline_->binding_point());
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
      /*field_of_view=*/40.0f, /*aspect_ratio=*/1.0f};
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
