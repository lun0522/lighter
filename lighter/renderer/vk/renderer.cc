//
//  renderer.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/renderer.h"

#include <array>
#include <unordered_map>

#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/image_util.h"
#include "lighter/renderer/vk/type_mapping.h"

namespace lighter::renderer::vk {
namespace {

constexpr int kNumFramesInFlight = 2;

intl::Image GetVkImage(const Image& image, uint32_t swapchain_image_index) {
  return image.IsSingleImage()
             ? *SingleImage::Cast(image)
             : MultiImage::Cast(image).image(swapchain_image_index);
}

intl::ImageView GetVkImageView(const Image& image,
                               uint32_t swapchain_image_index) {
  return image.IsSingleImage()
             ? SingleImage::Cast(image).image_view()
             : MultiImage::Cast(image).image_view(swapchain_image_index);
}

class VkCommandRecorder : public ir::CommandRecorder {
 public:
  VkCommandRecorder(intl::CommandBuffer command_buffer,
                    std::span<const std::unique_ptr<Pipeline>> pipelines)
      : command_buffer_{command_buffer}, pipelines_{pipelines} {}

  void BindPipeline(int pipeline_index) override {
    ASSERT_TRUE(pipeline_index >= 0 &&
                    static_cast<size_t>(pipeline_index) < pipelines_.size(),
                std::format("Pipeline index {} out of range", pipeline_index));
    bound_pipeline_ = pipelines_[pipeline_index].get();
    bound_pipeline_->Bind(command_buffer_);
  }

  void BindVertexBuffer(uint32_t binding_point, const ir::Buffer& buffer,
                        size_t offset) override {
    const auto& vk_buffer = static_cast<const Buffer&>(buffer);
    command_buffer_.bindVertexBuffers(binding_point, vk_buffer.vk_buffer(),
                                      static_cast<intl::DeviceSize>(offset));
  }

  void PushConstants(ir::shader_stage::ShaderStage stage, uint32_t offset,
                     uint32_t size, const void* data) override {
    ASSERT_NON_NULL(bound_pipeline_, "No pipeline bound before PushConstants");
    bound_pipeline_->PushConstants(
        command_buffer_, type::ConvertShaderStages(stage), offset, size, data);
  }

  void Draw(uint32_t vertex_count, uint32_t instance_count,
            uint32_t first_vertex, uint32_t first_instance) override {
    command_buffer_.draw(vertex_count, instance_count, first_vertex,
                         first_instance);
  }

 private:
  const intl::CommandBuffer command_buffer_;
  const std::span<const std::unique_ptr<Pipeline>> pipelines_;
  const Pipeline* bound_pipeline_ = nullptr;
};

class DynamicRenderPass : public ir::RenderPass {
 public:
  explicit DynamicRenderPass(const SharedContext& context,
                             ir::RenderPassDescriptor&& descriptor)
      : descriptor_{std::move(descriptor)} {
    const ShaderModule::AutoReleaseShaderPool shader_pool;
    for (const auto& subpass : descriptor_.subpass_descriptors) {
      Pipeline::RenderingInfo rendering_info;
      rendering_info.color_attachment_formats.reserve(
          subpass.color_attachments.size());
      intl::SampleCountFlagBits sample_count = intl::SampleCountFlagBits::e1;
      for (const ir::Image* attachment : subpass.color_attachments) {
        const auto& vk_image = Image::Cast(*attachment);
        rendering_info.color_attachment_formats.push_back(vk_image.format());
        sample_count = vk_image.sample_count();
      }
      if (subpass.depth_stencil_attachment != nullptr) {
        const auto& vk_ds = Image::Cast(*subpass.depth_stencil_attachment);
        rendering_info.depth_attachment_format = vk_ds.format();
        if (vk_ds.GetAspectFlags() & intl::ImageAspectFlagBits::eStencil) {
          rendering_info.stencil_attachment_format = vk_ds.format();
        }
        if (subpass.color_attachments.empty()) {
          sample_count = vk_ds.sample_count();
        }
      }
      for (const auto& pipeline_desc : subpass.pipeline_descriptors) {
        pipelines_.push_back(std::make_unique<Pipeline>(
            context, pipeline_desc, sample_count, rendering_info));
      }
    }
  }

  void RecordFrame(
      intl::CommandBuffer cmd, const Swapchain& swapchain,
      uint32_t swapchain_image_index,
      const std::function<void(ir::CommandRecorder&)>& record_draw) const {
    const intl::Extent2D render_extent = swapchain.image().extent();
    const intl::Rect2D render_area{{0, 0}, render_extent};

    ASSERT_TRUE(descriptor_.subpass_descriptors.size() == 1,
                "DynamicRenderPass currently supports single-subpass passes");
    const ir::SubpassDescriptor& subpass = descriptor_.subpass_descriptors[0];

    std::unordered_map<const ir::Image*, const ir::Image*> resolve_targets;
    for (const auto& resolve : subpass.multisample_resolves) {
      resolve_targets[resolve.source_image] = resolve.target_image;
    }

    std::vector<intl::RenderingAttachmentInfo> color_attachments;
    color_attachments.reserve(subpass.color_attachments.size());
    for (const ir::Image* attachment_ptr : subpass.color_attachments) {
      const auto& vk_color = Image::Cast(*attachment_ptr);
      image::TransitionImageLayout(
          cmd, GetVkImage(vk_color, swapchain_image_index),
          vk_color.GetAspectFlags(), /*base_mip_level=*/0,
          CAST_TO_UINT(vk_color.mip_levels()),
          CAST_TO_UINT(vk_color.GetNumLayers()), intl::ImageLayout::eUndefined,
          intl::ImageLayout::eColorAttachmentOptimal,
          intl::PipelineStageFlagBits2::eColorAttachmentOutput, {},
          intl::PipelineStageFlagBits2::eColorAttachmentOutput,
          intl::AccessFlagBits2::eColorAttachmentWrite |
              intl::AccessFlagBits2::eColorAttachmentRead);

      const auto ops_iter = descriptor_.color_ops_map.find(attachment_ptr);
      ASSERT_TRUE(ops_iter != descriptor_.color_ops_map.end(),
                  "Missing color load/store ops for attachment");
      const auto& ops = ops_iter->second;

      auto color_info =
          intl::RenderingAttachmentInfo{}
              .setImageView(GetVkImageView(vk_color, swapchain_image_index))
              .setImageLayout(intl::ImageLayout::eColorAttachmentOptimal)
              .setLoadOp(type::ConvertAttachmentLoadOp(ops.load_op))
              .setStoreOp(type::ConvertAttachmentStoreOp(ops.store_op))
              .setClearValue(intl::ClearColorValue{
                  std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}});

      const auto resolve_iter = resolve_targets.find(attachment_ptr);
      if (resolve_iter != resolve_targets.end()) {
        const auto& vk_resolve = Image::Cast(*resolve_iter->second);
        image::TransitionImageLayout(
            cmd, GetVkImage(vk_resolve, swapchain_image_index),
            vk_resolve.GetAspectFlags(), /*base_mip_level=*/0,
            CAST_TO_UINT(vk_resolve.mip_levels()),
            CAST_TO_UINT(vk_resolve.GetNumLayers()),
            intl::ImageLayout::eUndefined,
            intl::ImageLayout::eColorAttachmentOptimal,
            intl::PipelineStageFlagBits2::eColorAttachmentOutput, {},
            intl::PipelineStageFlagBits2::eColorAttachmentOutput,
            intl::AccessFlagBits2::eColorAttachmentWrite);
        color_info.setResolveMode(intl::ResolveModeFlagBits::eAverage)
            .setResolveImageView(
                GetVkImageView(vk_resolve, swapchain_image_index))
            .setResolveImageLayout(intl::ImageLayout::eColorAttachmentOptimal);
      }
      color_attachments.push_back(color_info);
    }

    intl::RenderingAttachmentInfo depth_info;
    intl::RenderingAttachmentInfo stencil_info;
    bool has_depth = false;
    bool has_stencil = false;
    if (subpass.depth_stencil_attachment != nullptr) {
      const auto& vk_ds = Image::Cast(*subpass.depth_stencil_attachment);
      image::TransitionImageLayout(
          cmd, GetVkImage(vk_ds, swapchain_image_index), vk_ds.GetAspectFlags(),
          /*base_mip_level=*/0, CAST_TO_UINT(vk_ds.mip_levels()),
          CAST_TO_UINT(vk_ds.GetNumLayers()), intl::ImageLayout::eUndefined,
          intl::ImageLayout::eDepthStencilAttachmentOptimal,
          intl::PipelineStageFlagBits2::eEarlyFragmentTests |
              intl::PipelineStageFlagBits2::eLateFragmentTests,
          {},
          intl::PipelineStageFlagBits2::eEarlyFragmentTests |
              intl::PipelineStageFlagBits2::eLateFragmentTests,
          intl::AccessFlagBits2::eDepthStencilAttachmentWrite |
              intl::AccessFlagBits2::eDepthStencilAttachmentRead);
      const auto ds_ops_iter = descriptor_.depth_stencil_ops_map.find(
          subpass.depth_stencil_attachment);
      ASSERT_TRUE(ds_ops_iter != descriptor_.depth_stencil_ops_map.end(),
                  "Missing depth/stencil load/store ops");
      const auto& ds_ops = ds_ops_iter->second;
      const auto aspect = vk_ds.GetAspectFlags();
      has_depth = static_cast<bool>(aspect & intl::ImageAspectFlagBits::eDepth);
      has_stencil =
          static_cast<bool>(aspect & intl::ImageAspectFlagBits::eStencil);
      if (has_depth) {
        depth_info =
            intl::RenderingAttachmentInfo{}
                .setImageView(GetVkImageView(vk_ds, swapchain_image_index))
                .setImageLayout(
                    intl::ImageLayout::eDepthStencilAttachmentOptimal)
                .setLoadOp(
                    type::ConvertAttachmentLoadOp(ds_ops.depth_ops.load_op))
                .setStoreOp(
                    type::ConvertAttachmentStoreOp(ds_ops.depth_ops.store_op))
                .setClearValue(intl::ClearDepthStencilValue{1.0f, 0});
      }
      if (has_stencil) {
        stencil_info =
            intl::RenderingAttachmentInfo{}
                .setImageView(GetVkImageView(vk_ds, swapchain_image_index))
                .setImageLayout(
                    intl::ImageLayout::eDepthStencilAttachmentOptimal)
                .setLoadOp(
                    type::ConvertAttachmentLoadOp(ds_ops.stencil_ops.load_op))
                .setStoreOp(
                    type::ConvertAttachmentStoreOp(ds_ops.stencil_ops.store_op))
                .setClearValue(intl::ClearDepthStencilValue{1.0f, 0});
      }
    }

    const auto rendering_info =
        intl::RenderingInfo{}
            .setRenderArea(render_area)
            .setLayerCount(1)
            .setColorAttachments(color_attachments)
            .setPDepthAttachment(has_depth ? &depth_info : nullptr)
            .setPStencilAttachment(has_stencil ? &stencil_info : nullptr);
    cmd.beginRendering(rendering_info);

    const auto viewport =
        intl::Viewport{}
            .setX(0.0f)
            .setY(static_cast<float>(render_extent.height))
            .setWidth(static_cast<float>(render_extent.width))
            .setHeight(-static_cast<float>(render_extent.height))
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f);
    const intl::Rect2D scissor = render_area;
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    VkCommandRecorder recorder{cmd, pipelines_};
    record_draw(recorder);

    cmd.endRendering();

    image::TransitionImageLayout(
        cmd, swapchain.image().image(swapchain_image_index),
        intl::ImageAspectFlagBits::eColor, /*base_mip_level=*/0,
        /*level_count=*/1, /*layer_count=*/1,
        intl::ImageLayout::eColorAttachmentOptimal,
        intl::ImageLayout::ePresentSrcKHR,
        intl::PipelineStageFlagBits2::eColorAttachmentOutput,
        intl::AccessFlagBits2::eColorAttachmentWrite,
        intl::PipelineStageFlagBits2::eNone, {});
  }

 private:
  const ir::RenderPassDescriptor descriptor_;
  std::vector<std::unique_ptr<Pipeline>> pipelines_;
};

std::vector<const common::Window*> ToConstWindowPtrs(
    std::span<common::Window* const> windows) {
  return {windows.begin(), windows.end()};
}

}  // namespace

Renderer::Renderer(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::vector<common::Window*>&& window_ptrs)
    : WithSharedContext{Context::CreateContext(application_name,
                                               debug_message_config,
                                               ToConstWindowPtrs(window_ptrs))},
      ir::Renderer{std::move(window_ptrs)} {
  swapchains_.resize(num_windows());
  per_frame_commands_.resize(num_windows());
  current_frames_.resize(num_windows(), 0);
  for (int i = 0; i < num_windows(); ++i) {
    swapchains_[i] = std::make_unique<Swapchain>(context_, i, *windows().at(i));
    per_frame_commands_[i] =
        std::make_unique<PerFrameCommand>(context_, kNumFramesInFlight);
  }
}

void Renderer::RecreateSwapchain(int window_index) {
  context_->WaitIdle();
  common::Window& window = mutable_window(window_index);
  window.Recreate();
  swapchains_[window_index] =
      std::make_unique<Swapchain>(context_, window_index, window);
}

std::unique_ptr<ir::RenderPass> Renderer::CreateRenderPass(
    ir::RenderPassDescriptor&& descriptor) const {
  return std::make_unique<DynamicRenderPass>(context_, std::move(descriptor));
}

bool Renderer::RenderFrame(
    int window_index, ir::RenderPass& pass,
    const std::function<void(ir::CommandRecorder&)>& record_draw) {
  const common::Window& window = *windows().at(window_index);
  const Swapchain& swapchain = *swapchains_.at(window_index);
  if (window.is_resized() ||
      window.GetFrameSize() != util::ToVec(swapchain.image().extent())) {
    RecreateSwapchain(window_index);
    return false;
  }

  auto& dynamic_pass = static_cast<DynamicRenderPass&>(pass);
  const int frame_index = current_frames_[window_index];
  const bool presented = per_frame_commands_[window_index]->Run(
      frame_index, swapchain,
      [&](intl::CommandBuffer cmd, uint32_t image_index) {
        dynamic_pass.RecordFrame(cmd, swapchain, image_index, record_draw);
      });
  current_frames_[window_index] = (frame_index + 1) % kNumFramesInFlight;

  if (!presented) {
    RecreateSwapchain(window_index);
    return false;
  }
  return true;
}

}  // namespace lighter::renderer::vk
