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

ViewerRenderer::ViewerRenderer(const SharedBasicContext& context,
                               int num_frames_in_flight,
                               const SamplableImage& aurora_paths_image,
                               const SamplableImage& distance_field_image) {
  using common::Vertex2DPosOnly;

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
  const auto vertex_data =
      Vertex2DPosOnly::GetFullScreenSquadVertices(/*flip_y=*/false);
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
}

void ViewerRenderer::UpdateFramebuffer(const VkExtent2D& frame_size,
                                       const RenderPass& render_pass,
                                       uint32_t subpass_index) {
  (*pipeline_builder_)
      .SetViewport(pipeline::GetFullFrameViewport(frame_size))
      .SetRenderPass(*render_pass, subpass_index);
  pipeline_ = pipeline_builder_->Build();
}

void ViewerRenderer::UpdateData(int frame, const common::Camera& camera) {
  auto& camera_parameter = *camera_uniform_->HostData<CameraParameter>(frame);
  camera_parameter.pos = glm::vec4{camera.position(), 0.0f};
  camera_parameter.up = glm::vec4{glm::cross(camera.right(), camera.front()),
                                  0.0f};
  camera_parameter.front = glm::vec4{camera.front(), 0.0f};
  camera_parameter.right = glm::vec4{camera.right(), 0.0f};
  camera_uniform_->Flush(frame);
}

void ViewerRenderer::Draw(const VkCommandBuffer& command_buffer,
                          int frame) const {
  pipeline_->Bind(command_buffer);
  descriptors_[frame]->Bind(command_buffer, pipeline_->layout(),
                            pipeline_->binding_point());
  vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                       /*mesh_index=*/0, /*instance_count=*/1);
}

Viewer::Viewer(
    WindowContext* window_context, int num_frames_in_flight,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : window_context_{*FATAL_IF_NULL(window_context)},
      path_dumper_{window_context_.basic_context(),
                   /*paths_image_dimension=*/1024,
                   std::move(aurora_paths_vertex_buffers)},
      viewer_renderer_{window_context_.basic_context(), num_frames_in_flight,
                       path_dumper_.aurora_paths_image(),
                       path_dumper_.distance_field_image()} {
  /* Camera */
  common::Camera::Config config;
  // We assume that:
  // (1) Aurora paths points are on a unit sphere.
  // (2) The camera is located at the center of sphere.
  // The 'look_at' point doesn't matter at this moment. It will be updated to
  // user viewpoint.
  config.far = 2.0f;
  config.up = GetEarthModelAxis();
  config.position = glm::vec3{0.0f};
  config.look_at = glm::vec3{1.0f};
  const common::PerspectiveCamera::PersConfig pers_config{
      /*field_of_view=*/30.0f, /*fov_aspect_ratio=*/1.0f};
  camera_ = absl::make_unique<common::PerspectiveCamera>(config, pers_config);

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

void Viewer::UpdateAuroraPaths(const glm::vec3& viewpoint_position) {
  camera_->UpdatePosition(viewpoint_position);
  camera_->UpdateDirection(viewpoint_position);
  path_dumper_.DumpAuroraPaths(*camera_);
}

void Viewer::OnEnter() {
  // TODO: Find a better way to exit viewer.
  did_press_right_ = false;
  window_context_.mutable_window()->RegisterMouseButtonCallback(
      [this](bool is_left, bool is_press) {
        did_press_right_ = !is_left && is_press;
      });
}

void Viewer::OnExit() {
  window_context_.mutable_window()->RegisterMouseButtonCallback(nullptr);
}

void Viewer::Recreate() {
  render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder_->color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context_.swapchain_image(framebuffer_index);
      });
  render_pass_ = (*render_pass_builder_)->Build();
  viewer_renderer_.UpdateFramebuffer(window_context_.frame_size(),
                                     *render_pass_, kViewImageSubpassIndex);
}

void Viewer::UpdateData(int frame) {
  viewer_renderer_.UpdateData(frame, *camera_);
}

void Viewer::Draw(const VkCommandBuffer& command_buffer,
                  uint32_t framebuffer_index, int current_frame) {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
        [this, current_frame](const VkCommandBuffer& command_buffer) {
          viewer_renderer_.Draw(command_buffer, current_frame);
        },
  });
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
