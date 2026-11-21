//
//  troop.cc
//
//  Created by Pujun Lun on 5/4/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <vector>

#include "lighter/scene/model.h"
#include "lighter/scene/util.h"

namespace lighter::example {
namespace {

using base::Vertex2D;
using base::Vertex3DPosOnly;
using renderer::ir::AccessLocation;
using renderer::ir::AttachmentLoadOp;
using renderer::ir::AttachmentStoreOp;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::CompareOp;
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
using renderer::ir::SamplerDescriptor;
using renderer::ir::TransientImageDesc;
using renderer::ir::TransientImageFormat;
using renderer::ir::VertexInputRate;
using scene::Model;
using scene::ModelBuilder;

constexpr int kWindowIndex = 0;
constexpr int kNumFramesInFlight = 2;
constexpr int kNumLights = 32;
constexpr int kObjFileIndexBase = 1;
constexpr uint32_t kStencilReference = 0xFF;

// Consistent with uniform blocks defined in shaders.
struct GeometryTrans {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 model_inv_trs;
  ALIGN_MAT4 glm::mat4 proj_view;
};

struct Lights {
  ALIGN_VEC4 glm::vec4 colors[kNumLights];
};

struct RenderInfo {
  ALIGN_VEC4 glm::vec4 light_centers[kNumLights];
  ALIGN_VEC4 glm::vec4 camera_pos;
};

struct LightCubeTrans {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 proj_view;
};

struct LightCenterConfig {
  glm::vec2 bound_x;
  glm::vec2 bound_y;
  glm::vec2 bound_z;
  glm::vec3 increments;
};

float WrapAroundCoordinate(float coord, const glm::vec2& bound) {
  ASSERT_TRUE(bound.x <= bound.y,
              "bound.x and bound.y should be lower and upper bound");
  if (bound.x == bound.y) {
    return bound.x;
  }
  if (coord < bound.x) {
    return bound.y - glm::mod(coord - bound.x, bound.y - bound.x);
  }
  if (coord >= bound.y) {
    return bound.x + glm::mod(coord - bound.y, bound.y - bound.x);
  }
  return coord;
}

std::vector<glm::vec3> GenerateOriginalLightCenters(
    const LightCenterConfig& config) {
  std::random_device device;
  std::mt19937 rand_gen{device()};
  std::uniform_real_distribution<float> center_x{config.bound_x.x,
                                                 config.bound_x.y};
  std::uniform_real_distribution<float> center_y{config.bound_y.x,
                                                 config.bound_y.y};
  std::uniform_real_distribution<float> center_z{config.bound_z.x,
                                                 config.bound_z.y};

  std::vector<glm::vec3> centers(kNumLights);
  std::generate(centers.begin(), centers.end(), [&]() {
    return glm::vec3{center_x(rand_gen), center_y(rand_gen),
                     center_z(rand_gen)};
  });
  return centers;
}

class TroopExample {
 public:
  explicit TroopExample(const glm::ivec2& screen_size)
      : light_center_config_{
            .bound_x = {-3.0f, 9.8f},
            .bound_y = {3.2f, 4.2f},
            .bound_z = {-12.0f, 3.0f},
            .increments = {0.0f, 0.0f, 2.0f},
        },
        original_light_centers_{
            GenerateOriginalLightCenters(light_center_config_)},
        window_{"Troop", screen_size} {
    using base::file::GetResourcePath;
    using TextureType = ModelBuilder::TextureType;

    renderer_ = scene::CreateRenderer("Troop Example", {&window_});

    const float aspect_ratio = window_.original_aspect_ratio();
    scene::Camera::Config camera_config;
    camera_config.position = glm::vec3{8.5f, 5.5f, 5.0f};
    camera_config.look_at = glm::vec3{8.0f, 5.0f, 4.22f};
    camera_ = scene::UserControlledPerspectiveCamera::Create(
        /*control_config=*/{}, camera_config,
        {/*field_of_view_y=*/45.0f, aspect_ratio});
    scene::RegisterCameraControls(window_, timer_, *camera_, &should_quit_);

    // Soldier formation instances.
    constexpr glm::ivec2 kNumSoldiersGrid{5, 10};
    constexpr glm::vec2 kSoldierInterval{1.7f, -1.0f};
    num_soldiers_ = kNumSoldiersGrid.x * kNumSoldiersGrid.y;
    std::vector<glm::vec3> centers;
    centers.reserve(static_cast<size_t>(num_soldiers_));
    for (int x = 0; x < kNumSoldiersGrid.x; ++x) {
      for (int z = 0; z < kNumSoldiersGrid.y; ++z) {
        centers.push_back(glm::vec3{
            kSoldierInterval.x * static_cast<float>(x),
            0.0f,
            kSoldierInterval.y * static_cast<float>(z),
        });
      }
    }
    const size_t centers_bytes =
        base::util::GetTotalDataSize(std::span<const glm::vec3>{centers});
    soldier_centers_buffer_ =
        renderer_->CreateBuffer(Buffer::UpdateRate::kLow, centers_bytes,
                                std::array{BufferUsage::GetVertexBufferUsage(
                                    BufferUsage::UsageType::kVertexOnly)});
    soldier_centers_buffer_->CopyToDevice(std::array{
        Buffer::CopyInfo{centers.data(), centers_bytes, /*offset=*/0}});

    const std::array vert_uniform_usages{
        BufferUsage::GetUniformBufferUsage(AccessLocation::kVertexShader)};
    const std::array shared_uniform_usages{
        BufferUsage::GetUniformBufferUsage(AccessLocation::kVertexShader),
        BufferUsage::GetUniformBufferUsage(AccessLocation::kFragmentShader),
    };
    for (int i = 0; i < kNumFramesInFlight; ++i) {
      geometry_uniforms_[i] =
          renderer_->CreateBuffer(Buffer::UpdateRate::kHigh,
                                  sizeof(GeometryTrans), vert_uniform_usages);
      render_info_uniforms_[i] = renderer_->CreateBuffer(
          Buffer::UpdateRate::kHigh, sizeof(RenderInfo), shared_uniform_usages);
    }

    // Light colors (generated once at startup).
    std::random_device device;
    std::mt19937 rand_gen{device()};
    std::uniform_real_distribution<float> color_dist{0.5f, 1.0f};
    Lights lights{};
    for (auto& color : lights.colors) {
      color = glm::vec4{color_dist(rand_gen), color_dist(rand_gen),
                        color_dist(rand_gen), 0.0f};
    }
    lights_colors_uniform_ = renderer_->CreateBuffer(
        Buffer::UpdateRate::kLow, sizeof(Lights), shared_uniform_usages);
    lights_colors_uniform_->CopyToDevice(
        std::array{Buffer::CopyInfo{&lights, sizeof(Lights), /*offset=*/0}});

    // Soldier model for geometry pass.
    nanosuit_model_ =
        ModelBuilder{
            *renderer_, "GeometryPass", aspect_ratio,
            ModelBuilder::MultiMeshResource{
                /*model_path=*/GetResourcePath("model/nanosuit/nanosuit.obj"),
                /*texture_dir=*/
                GetResourcePath("model/nanosuit/nanosuit.obj",
                                /*want_directory_path=*/true),
            }}
            .AddTextureBindingPoint(TextureType::kDiffuse, /*binding=*/1)
            .AddTextureBindingPoint(TextureType::kSpecular, /*binding=*/2)
            .AddTextureBindingPoint(TextureType::kReflection, /*binding=*/3)
            .AddPerInstanceBuffer(
                renderer::ir::buffer::CreateAttributesForVertex3DPosOnly(0),
                sizeof(glm::vec3), soldier_centers_buffer_.get())
            .SetShader(
                renderer::ir::shader_stage::VERTEX,
                base::file::GetShaderBinaryPath("troop/geometry_pass.vert"))
            .SetShader(
                renderer::ir::shader_stage::FRAGMENT,
                base::file::GetShaderBinaryPath("troop/geometry_pass.frag"))
            .Build();

    // Cube geometry for point light indicators.
    const base::ObjFilePosOnly cube_file{GetResourcePath("model/cube.obj"),
                                         kObjFileIndexBase};
    cube_index_count_ = static_cast<uint32_t>(cube_file.indices.size());
    const size_t cube_vertex_bytes = base::util::GetTotalDataSize(
        std::span<const Vertex3DPosOnly>{cube_file.vertices});
    const size_t cube_index_bytes = base::util::GetTotalDataSize(
        std::span<const uint32_t>{cube_file.indices});
    cube_index_offset_ = cube_vertex_bytes;
    cube_buffer_ = renderer_->CreateBuffer(
        Buffer::UpdateRate::kLow, cube_vertex_bytes + cube_index_bytes,
        std::array{BufferUsage::GetVertexBufferUsage(
            BufferUsage::UsageType::kIndexAndVertex)});
    cube_buffer_->CopyToDevice(std::array{
        Buffer::CopyInfo{cube_file.vertices.data(), cube_vertex_bytes,
                         /*offset=*/0},
        Buffer::CopyInfo{cube_file.indices.data(), cube_index_bytes,
                         cube_index_offset_},
    });

    // Full-screen quad for deferred lighting pass.
    const std::array<Vertex2D, 6> quad_vertices =
        Vertex2D::GetFullScreenSquadVertices(/*flip_y=*/true);
    const size_t quad_bytes =
        base::util::GetTotalDataSize(std::span<const Vertex2D>{quad_vertices});
    quad_buffer_ =
        renderer_->CreateBuffer(Buffer::UpdateRate::kLow, quad_bytes,
                                std::array{BufferUsage::GetVertexBufferUsage(
                                    BufferUsage::UsageType::kVertexOnly)});
    quad_buffer_->CopyToDevice(std::array{
        Buffer::CopyInfo{quad_vertices.data(), quad_bytes, /*offset=*/0}});

    base_lights_pipeline_ =
        GraphicsPipelineDescriptor{}
            .SetName("Lights")
            .SetShader(renderer::ir::shader_stage::VERTEX,
                       base::file::GetShaderBinaryPath("troop/light_cube.vert"))
            .SetShader(renderer::ir::shader_stage::FRAGMENT,
                       base::file::GetShaderBinaryPath("troop/light_cube.frag"))
            .UseColorAttachment(/*location=*/0, std::nullopt)
            .EnableDepthTestAndWrite()
            .EnableStencilTest(
                renderer::ir::pipeline::GetStencilWrite(kStencilReference),
                renderer::ir::pipeline::GetStencilNop())
            .AddVertexInput({
                VertexInputRate::kVertex,
                /*binding_point=*/0,
                /*stride=*/sizeof(Vertex3DPosOnly),
                renderer::ir::buffer::CreateAttributesForVertex3DPosOnly(0),
            });

    base_soldiers_pipeline_ =
        GraphicsPipelineDescriptor{}
            .SetName("Soldiers")
            .SetShader(
                renderer::ir::shader_stage::VERTEX,
                base::file::GetShaderBinaryPath("troop/lighting_pass.vert"))
            .SetShader(
                renderer::ir::shader_stage::FRAGMENT,
                base::file::GetShaderBinaryPath("troop/lighting_pass.frag"))
            .UseColorAttachment(/*location=*/0, std::nullopt)
            .EnableStencilTest(renderer::ir::pipeline::GetStencilRead(
                                   CompareOp::kNotEqual, kStencilReference),
                               renderer::ir::pipeline::GetStencilNop())
            .AddVertexInput({
                VertexInputRate::kVertex,
                /*binding_point=*/0,
                /*stride=*/sizeof(Vertex2D),
                renderer::ir::buffer::CreateAttributesForVertex2D(
                    /*loc_pos=*/0, /*loc_tex_coord=*/1),
            });
  }

  // This class is neither copyable nor movable.
  TroopExample(const TroopExample&) = delete;
  TroopExample& operator=(const TroopExample&) = delete;

  void MainLoop() {
    constexpr float kSoldierModelScale = 0.2f;
    constexpr float kLightCubeScale = 0.1f;
    const glm::mat4 soldier_model =
        glm::scale(glm::mat4{1.0f}, glm::vec3{kSoldierModelScale});
    const glm::mat4 soldier_model_inv_trs =
        glm::transpose(glm::inverse(soldier_model));
    const glm::mat4 light_cube_model =
        glm::scale(glm::mat4{1.0f}, glm::vec3{kLightCubeScale});
    const SamplerDescriptor gbuffer_sampler{
        .filter_type = FilterType::kNearest,
        .address_mode = SamplerAddressMode::kClampToEdge,
    };

    camera_->SetCursorPos(window_.GetCursorPos());
    RenderGraph graph;

    while (!should_quit_ && !window_.ShouldQuit()) {
      window_.ProcessUserInputs();
      timer_.Tick();

      const scene::Camera& cam = camera_->camera();
      const glm::mat4 proj_view =
          cam.GetProjectionMatrix() * cam.GetViewMatrix();
      const GeometryTrans geom_trans{soldier_model, soldier_model_inv_trs,
                                     proj_view};
      const LightCubeTrans light_cube_trans{light_cube_model, proj_view};

      RenderInfo render_info{};
      render_info.camera_pos = glm::vec4{cam.position(), 0.0f};
      const glm::vec3 delta =
          light_center_config_.increments * timer_.GetElapsedTimeSinceLaunch();
      for (int i = 0; i < kNumLights; ++i) {
        const glm::vec3 center = original_light_centers_[i] + delta;
        render_info.light_centers[i] = {
            WrapAroundCoordinate(center.x, light_center_config_.bound_x),
            WrapAroundCoordinate(center.y, light_center_config_.bound_y),
            WrapAroundCoordinate(center.z, light_center_config_.bound_z),
            0.0f,
        };
      }

      graph.Reset();
      const ImageHandle backbuffer = graph.ImportSwapchainImage(kWindowIndex);

      // Pass 1: Geometry pass (3 G-buffer targets + preserved depth/stencil).
      ImageHandle gbuffer_position;
      ImageHandle gbuffer_normal;
      ImageHandle gbuffer_diff_spec;
      ImageHandle depth_stencil;
      graph.AddGraphicsPass(
          "GeometryPass",
          [&](PassBuilder& builder) {
            gbuffer_position = builder.CreateImage(TransientImageDesc{
                .name = "gbuffer_position",
                .format = TransientImageFormat::kColorFloat16,
                .multisampling_mode = MultisamplingMode::kNone,
                .match_image = backbuffer,
            });
            gbuffer_normal = builder.CreateImage(TransientImageDesc{
                .name = "gbuffer_normal",
                .format = TransientImageFormat::kColorFloat16,
                .multisampling_mode = MultisamplingMode::kNone,
                .match_image = backbuffer,
            });
            gbuffer_diff_spec = builder.CreateImage(TransientImageDesc{
                .name = "gbuffer_diff_spec",
                .format = TransientImageFormat::kColorUnorm8,
                .multisampling_mode = MultisamplingMode::kNone,
                .match_image = backbuffer,
            });
            depth_stencil = builder.CreateImage(TransientImageDesc{
                .name = "depth_stencil",
                .format = TransientImageFormat::kDepthStencil,
                .multisampling_mode = MultisamplingMode::kNone,
                .match_image = backbuffer,
            });

            gbuffer_position = builder.ColorAttachment(gbuffer_position, 0);
            gbuffer_normal = builder.ColorAttachment(gbuffer_normal, 1);
            gbuffer_diff_spec = builder.ColorAttachment(gbuffer_diff_spec, 2);
            depth_stencil =
                builder.DepthAttachment(depth_stencil, AttachmentLoadOp::kClear,
                                        AttachmentStoreOp::kStore);
          },
          [&](PassContext& ctx) {
            const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
            const Buffer& geom_uniform = *geometry_uniforms_[frame_slot_];
            geom_uniform.CopyToDevice(std::array{Buffer::CopyInfo{
                &geom_trans, sizeof(GeometryTrans), /*offset=*/0}});

            nanosuit_model_->BindPipeline(ctx, frame_size,
                                          /*is_object_opaque=*/true,
                                          /*flip_viewport_y=*/true,
                                          /*num_color_attachments=*/3);
            ctx.BindBuffer(/*binding=*/0, geom_uniform);
            nanosuit_model_->Draw(ctx, static_cast<uint32_t>(num_soldiers_));
          });

      // Pass 2: Deferred lighting pass (lights write stencil; soldiers read
      // stencil != 0xFF and shade from G-buffers).
      graph.AddGraphicsPass(
          "LightingPass",
          [&](PassBuilder& builder) {
            gbuffer_position =
                builder.Read(gbuffer_position,
                             ImageUsage::GetSampledInFragmentShaderUsage());
            gbuffer_normal = builder.Read(
                gbuffer_normal, ImageUsage::GetSampledInFragmentShaderUsage());
            gbuffer_diff_spec =
                builder.Read(gbuffer_diff_spec,
                             ImageUsage::GetSampledInFragmentShaderUsage());

            builder.ColorAttachment(backbuffer, /*location=*/0);
            depth_stencil = builder.DepthAttachment(
                depth_stencil, AttachmentLoadOp::kLoad,
                AttachmentStoreOp::kDontCare, AttachmentLoadOp::kClear,
                AttachmentStoreOp::kDontCare, /*clear_depth=*/1.0f,
                /*clear_stencil=*/0);
          },
          [&](PassContext& ctx) {
            const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
            const Buffer& render_info_uniform =
                *render_info_uniforms_[frame_slot_];
            render_info_uniform.CopyToDevice(std::array{Buffer::CopyInfo{
                &render_info, sizeof(RenderInfo), /*offset=*/0}});

            GraphicsPipelineDescriptor lights_desc = base_lights_pipeline_;
            lights_desc.SetViewport(
                renderer::ir::pipeline::GetViewport(
                    frame_size, window_.original_aspect_ratio()),
                renderer::ir::pipeline::GetFullFrameScissor(frame_size),
                /*flip_y=*/true);
            ctx.BindGraphicsPipeline(lights_desc);
            ctx.BindBuffer(/*binding=*/0, *lights_colors_uniform_);
            ctx.BindBuffer(/*binding=*/1, render_info_uniform);
            ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                              sizeof(LightCubeTrans), &light_cube_trans);
            ctx.BindVertexBuffer(/*binding_point=*/0, *cube_buffer_);
            ctx.BindIndexBuffer(*cube_buffer_, cube_index_offset_,
                                /*is_uint16=*/false);
            ctx.DrawIndexed(cube_index_count_, kNumLights);

            GraphicsPipelineDescriptor soldiers_desc = base_soldiers_pipeline_;
            soldiers_desc.SetViewport(
                renderer::ir::pipeline::GetFullFrameViewport(frame_size),
                renderer::ir::pipeline::GetFullFrameScissor(frame_size),
                /*flip_y=*/true);
            ctx.BindGraphicsPipeline(soldiers_desc);
            ctx.BindBuffer(/*binding=*/0, *lights_colors_uniform_);
            ctx.BindBuffer(/*binding=*/1, render_info_uniform);
            ctx.BindImage(/*binding=*/2, gbuffer_position, &gbuffer_sampler);
            ctx.BindImage(/*binding=*/3, gbuffer_normal, &gbuffer_sampler);
            ctx.BindImage(/*binding=*/4, gbuffer_diff_spec, &gbuffer_sampler);
            ctx.BindVertexBuffer(/*binding_point=*/0, *quad_buffer_);
            ctx.Draw(/*vertex_count=*/6);
          });

      if (!renderer_->RenderFrame(kWindowIndex, graph)) {
        camera_->SetCursorPos(window_.GetCursorPos());
      }
      frame_slot_ = (frame_slot_ + 1) % kNumFramesInFlight;
      // Camera is not activated until the first frame is displayed.
      camera_->SetActivity(true);
    }
    renderer_->WaitIdle();
  }

 private:
  const LightCenterConfig light_center_config_;
  const std::vector<glm::vec3> original_light_centers_;

  bool should_quit_ = false;
  int frame_slot_ = 0;
  int num_soldiers_ = 0;
  uint32_t cube_index_count_ = 0;
  size_t cube_index_offset_ = 0;

  base::FrameTimer timer_;
  base::Window window_;
  std::unique_ptr<scene::UserControlledPerspectiveCamera> camera_;
  std::unique_ptr<Renderer> renderer_;

  std::unique_ptr<Buffer> soldier_centers_buffer_;
  std::unique_ptr<Model> nanosuit_model_;
  std::unique_ptr<Buffer> lights_colors_uniform_;
  std::array<std::unique_ptr<Buffer>, kNumFramesInFlight> geometry_uniforms_;
  std::array<std::unique_ptr<Buffer>, kNumFramesInFlight> render_info_uniforms_;
  std::unique_ptr<Buffer> cube_buffer_;
  std::unique_ptr<Buffer> quad_buffer_;

  GraphicsPipelineDescriptor base_lights_pipeline_;
  GraphicsPipelineDescriptor base_soldiers_pipeline_;
};

}  // namespace
}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::scene::RunApplication<lighter::example::TroopExample>(
      argc, argv, glm::ivec2{800, 600});
}
