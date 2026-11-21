//
//  render_graph.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/render_graph.h"

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

#include "lighter/base/util.h"
#include "lighter/renderer/ir/render_graph_util.h"
#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/buffer_util.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/image_util.h"
#include "lighter/renderer/vk/pipeline.h"
#include "lighter/renderer/vk/type_mapping.h"

namespace lighter::renderer::vk {
namespace {

using ir::BufferUsage;
using ir::ImageUsage;
using ir::TransientImageFormat;
using ir::render_graph::GraphvizBarrierNote;

struct ExecutionResourceState {
  std::vector<const Image*> resolved_images;
  std::vector<const Buffer*> resolved_buffers;
  std::unordered_map<const Image*, ImageUsage> current_image_usage;
  std::unordered_map<const Buffer*, BufferUsage> current_buffer_usage;
};

intl::Format ResolveTransientImageFormat(
    const Context& context, const ir::TransientImageDesc& desc,
    std::span<const Image* const> resolved_images) {
  switch (desc.format) {
    case TransientImageFormat::kColorUnorm8:
      return intl::Format::eR8G8B8A8Unorm;
    case TransientImageFormat::kColorFloat16:
      return intl::Format::eR16G16B16A16Sfloat;
    case TransientImageFormat::kDepthStencil:
      return OwnedImage::GetDepthStencilFormat(context);
    case TransientImageFormat::kMatchExternalImage: {
      const Image* match = resolved_images[desc.match_image.id];
      ASSERT_NON_NULL(match,
                      std::format("match_image for transient '{}' not resolved",
                                  desc.name));
      return match->format();
    }
  }
}

intl::Extent2D ResolveTransientImageExtent(
    const ir::TransientImageDesc& desc,
    std::span<const Image* const> resolved_images) {
  if (desc.extent.x > 0 && desc.extent.y > 0) {
    return util::ToExtent(desc.extent);
  }
  const Image* match = resolved_images[desc.match_image.id];
  ASSERT_NON_NULL(
      match,
      std::format("match_image for transient '{}' not resolved", desc.name));
  return match->extent();
}

intl::ImageSubresourceRange GetFullImageSubresourceRange(const Image& image) {
  return intl::ImageSubresourceRange{}
      .setAspectMask(image.GetAspectFlags())
      .setBaseMipLevel(0)
      .setLevelCount(CAST_TO_UINT(image.mip_levels()))
      .setBaseArrayLayer(0)
      .setLayerCount(CAST_TO_UINT(image.GetNumLayers()));
}

void FlushPipelineBarriers(
    intl::CommandBuffer cmd, std::span<const intl::ImageMemoryBarrier2> images,
    std::span<const intl::BufferMemoryBarrier2> buffers) {
  if (!images.empty() || !buffers.empty()) {
    cmd.pipelineBarrier2(intl::DependencyInfo{}
                             .setImageMemoryBarriers(images)
                             .setBufferMemoryBarriers(buffers));
  }
}

Pipeline::RenderingInfo BuildPassRenderingInfo(
    const ir::PassNode& pass, std::span<const Image* const> resolved_images,
    intl::SampleCountFlagBits* out_sample_count,
    intl::Extent2D* out_render_extent) {
  Pipeline::RenderingInfo rendering_info;
  rendering_info.color_attachment_formats.reserve(
      pass.color_attachments.size());
  *out_sample_count = intl::SampleCountFlagBits::e1;
  *out_render_extent = intl::Extent2D{0, 0};

  for (const ir::ColorAttachmentDesc& attachment : pass.color_attachments) {
    const Image* img = resolved_images[attachment.image.id];
    rendering_info.color_attachment_formats.push_back(img->format());
    *out_sample_count = img->sample_count();
    if (out_render_extent->width == 0) {
      *out_render_extent = img->extent();
    }
  }
  if (pass.depth_stencil_attachment.has_value()) {
    const Image* ds = resolved_images[pass.depth_stencil_attachment->image.id];
    rendering_info.depth_attachment_format = ds->format();
    if (ds->GetAspectFlags() & intl::ImageAspectFlagBits::eStencil) {
      rendering_info.stencil_attachment_format = ds->format();
    }
    if (pass.color_attachments.empty()) {
      *out_sample_count = ds->sample_count();
    }
    if (out_render_extent->width == 0) {
      *out_render_extent = ds->extent();
    }
  }
  return rendering_info;
}

template <typename Lifetime>
std::vector<uint32_t> CollectUsedTransientIdsSortedByFirstPass(
    const auto& nodes, std::span<const Lifetime> lifetimes) {
  std::vector<uint32_t> ids;
  for (size_t id = 0; id < nodes.size(); ++id) {
    if (!nodes[id].is_external && lifetimes[id].IsUsed()) {
      ids.push_back(static_cast<uint32_t>(id));
    }
  }
  std::sort(ids.begin(), ids.end(), [&](uint32_t a, uint32_t b) {
    return lifetimes[a].first_pass < lifetimes[b].first_pass;
  });
  return ids;
}

void ResolveExternalResources(
    const ir::RenderGraph& graph, std::span<const Swapchain* const> swapchains,
    std::span<const uint32_t> acquired_swapchain_image_indices,
    ExecutionResourceState* state) {
  const auto& ir_images = graph.images();
  const auto& ir_buffers = graph.buffers();
  state->resolved_images.assign(ir_images.size(), nullptr);
  state->resolved_buffers.assign(ir_buffers.size(), nullptr);

  for (size_t id = 0; id < ir_images.size(); ++id) {
    const ir::ImageResourceNode& node = ir_images[id];
    if (!node.is_external) {
      continue;
    }
    const Image* physical = nullptr;
    if (node.is_swapchain()) {
      const int win_idx = node.swapchain_window_index;
      ASSERT_TRUE(
          win_idx >= 0 && static_cast<size_t>(win_idx) < swapchains.size(),
          "Swapchain window_index out of range");
      physical = &swapchains[win_idx]->frame_image(
          acquired_swapchain_image_indices[win_idx]);
    } else {
      physical = &Image::Cast(*node.external_image);
    }
    state->resolved_images[id] = physical;
    state->current_image_usage[physical] = node.initial_usage;
  }

  for (size_t id = 0; id < ir_buffers.size(); ++id) {
    const ir::BufferResourceNode& node = ir_buffers[id];
    if (!node.is_external) {
      continue;
    }
    const Buffer* physical = &static_cast<const Buffer&>(*node.external_buffer);
    state->resolved_buffers[id] = physical;
    state->current_buffer_usage[physical] = node.initial_usage;
  }
}

void AcquireTransientResources(const Context& context,
                               const ir::RenderGraph& graph,
                               const ir::CompiledRenderGraph& compiled,
                               int frame_index,
                               TransientResourcePool* transient_pool,
                               ExecutionResourceState* state) {
  const auto& ir_images = graph.images();
  for (uint32_t id :
       CollectUsedTransientIdsSortedByFirstPass<ir::ResourceLifetime>(
           ir_images, compiled.image_lifetimes)) {
    const ir::ImageResourceNode& node = ir_images[id];
    const ir::TransientImageDesc& desc = *node.transient_desc;
    const TransientImageKey key{
        .extent = ResolveTransientImageExtent(desc, state->resolved_images),
        .format =
            ResolveTransientImageFormat(context, desc, state->resolved_images),
        .sample_count =
            context.physical_device().sample_count(desc.multisampling_mode),
        .usage_flags = image::GetImageUsageFlags(node.all_usages),
    };
    const auto acquired = transient_pool->AcquireImage(
        frame_index, node.name, key, desc.multisampling_mode,
        compiled.image_lifetimes[id]);
    state->resolved_images[id] = acquired.image;
    if (!acquired.aliased_in_same_frame) {
      state->current_image_usage[acquired.image] = ImageUsage{};
    }
  }

  const auto& ir_buffers = graph.buffers();
  for (uint32_t id :
       CollectUsedTransientIdsSortedByFirstPass<ir::ResourceLifetime>(
           ir_buffers, compiled.buffer_lifetimes)) {
    const ir::BufferResourceNode& node = ir_buffers[id];
    const TransientBufferKey key{
        .size = node.transient_desc->size,
        .usage_flags = buffer::GetBufferUsageFlags(node.all_usages),
    };
    const auto acquired = transient_pool->AcquireBuffer(
        frame_index, key, compiled.buffer_lifetimes[id]);
    state->resolved_buffers[id] = acquired.buffer;
    if (!acquired.aliased_in_same_frame) {
      state->current_buffer_usage[acquired.buffer] = BufferUsage{};
    }
  }
}

void RecordPassBarriers(intl::CommandBuffer cmd, const ir::PassNode& pass,
                        ExecutionResourceState* state,
                        std::vector<GraphvizBarrierNote>* barrier_notes) {
  std::vector<intl::ImageMemoryBarrier2> image_barriers;
  std::vector<intl::BufferMemoryBarrier2> buffer_barriers;

  for (const ir::ImageAccess& access : pass.image_accesses) {
    const Image* physical = state->resolved_images[access.handle.id];
    const ImageUsage prev_usage = state->current_image_usage[physical];
    if (const auto barrier = image::CreateImageMemoryBarrier(
            physical->vk_image(), GetFullImageSubresourceRange(*physical),
            prev_usage, access.usage)) {
      image_barriers.push_back(*barrier);
      barrier_notes->push_back(GraphvizBarrierNote{
          .is_image = true,
          .resource_id = access.handle.id,
          .version = access.handle.version,
          .consumer_pass = pass.index,
          .label = std::format("{} -> {}", intl::to_string(barrier->oldLayout),
                               intl::to_string(barrier->newLayout)),
      });
    }
    state->current_image_usage[physical] = access.usage;
  }

  for (const ir::BufferAccess& access : pass.buffer_accesses) {
    const Buffer* physical = state->resolved_buffers[access.handle.id];
    const BufferUsage prev_usage = state->current_buffer_usage[physical];
    if (const auto barrier = buffer::CreateBufferMemoryBarrier(
            physical->vk_buffer(), /*offset=*/0, physical->size(), prev_usage,
            access.usage)) {
      buffer_barriers.push_back(*barrier);
      barrier_notes->push_back(GraphvizBarrierNote{
          .is_image = false,
          .resource_id = access.handle.id,
          .version = access.handle.version,
          .consumer_pass = pass.index,
          .label =
              std::format("{} -> {}", intl::to_string(barrier->srcStageMask),
                          intl::to_string(barrier->dstStageMask)),
      });
    }
    state->current_buffer_usage[physical] = access.usage;
  }

  FlushPipelineBarriers(cmd, image_barriers, buffer_barriers);
}

void RecordFinalizationBarriers(
    intl::CommandBuffer cmd, const ir::RenderGraph& graph,
    const ExecutionResourceState& state,
    std::vector<GraphvizBarrierNote>* barrier_notes) {
  std::vector<intl::ImageMemoryBarrier2> image_barriers;
  std::vector<intl::BufferMemoryBarrier2> buffer_barriers;

  for (size_t id = 0; id < graph.images().size(); ++id) {
    const ir::ImageResourceNode& node = graph.images()[id];
    if (!node.is_external ||
        node.final_usage.usage_type() == ImageUsage::UsageType::kDontCare) {
      continue;
    }
    const Image* physical = state.resolved_images[id];
    const auto prev_iter = state.current_image_usage.find(physical);
    const ImageUsage prev_usage = prev_iter != state.current_image_usage.end()
                                      ? prev_iter->second
                                      : ImageUsage{};
    if (const auto barrier = image::CreateImageMemoryBarrier(
            physical->vk_image(), GetFullImageSubresourceRange(*physical),
            prev_usage, node.final_usage)) {
      image_barriers.push_back(*barrier);
      barrier_notes->push_back(GraphvizBarrierNote{
          .is_image = true,
          .resource_id = static_cast<uint32_t>(id),
          .version = node.current_version,
          .consumer_pass = ir::kInvalidResourceId,
          .label = std::format("{} -> {}", intl::to_string(barrier->oldLayout),
                               intl::to_string(barrier->newLayout)),
      });
    }
  }

  for (size_t id = 0; id < graph.buffers().size(); ++id) {
    const ir::BufferResourceNode& node = graph.buffers()[id];
    if (!node.is_external ||
        node.final_usage.usage_type() == BufferUsage::UsageType::kDontCare) {
      continue;
    }
    const Buffer* physical = state.resolved_buffers[id];
    const auto prev_iter = state.current_buffer_usage.find(physical);
    const BufferUsage prev_usage = prev_iter != state.current_buffer_usage.end()
                                       ? prev_iter->second
                                       : BufferUsage{};
    if (const auto barrier = buffer::CreateBufferMemoryBarrier(
            physical->vk_buffer(), /*offset=*/0, physical->size(), prev_usage,
            node.final_usage)) {
      buffer_barriers.push_back(*barrier);
      barrier_notes->push_back(GraphvizBarrierNote{
          .is_image = false,
          .resource_id = static_cast<uint32_t>(id),
          .version = node.current_version,
          .consumer_pass = ir::kInvalidResourceId,
          .label =
              std::format("{} -> {}", intl::to_string(barrier->srcStageMask),
                          intl::to_string(barrier->dstStageMask)),
      });
    }
  }

  FlushPipelineBarriers(cmd, image_barriers, buffer_barriers);
}

class VkPassContext : public ir::PassContext {
 public:
  VkPassContext(PipelineCache& pipeline_cache, intl::CommandBuffer cmd,
                const ir::PassNode& pass,
                std::span<const Image* const> resolved_images,
                std::span<const Buffer* const> resolved_buffers,
                intl::SampleCountFlagBits sample_count,
                const Pipeline::RenderingInfo& rendering_info)
      : pipeline_cache_{pipeline_cache},
        cmd_{cmd},
        pass_{pass},
        resolved_images_{resolved_images},
        resolved_buffers_{resolved_buffers},
        sample_count_{sample_count},
        rendering_info_{rendering_info} {}

  const ir::Image& GetImage(ir::ImageHandle handle) const override {
    const Image* img = resolved_images_[handle.id];
    ASSERT_NON_NULL(img, "ImageHandle did not resolve to a physical image");
    return *img;
  }

  const ir::Buffer& GetBuffer(ir::BufferHandle handle) const override {
    const Buffer* buf = resolved_buffers_[handle.id];
    ASSERT_NON_NULL(buf, "BufferHandle did not resolve to a physical buffer");
    return *buf;
  }

  glm::ivec2 GetImageExtent(ir::ImageHandle handle) const override {
    const Image* img = resolved_images_[handle.id];
    ASSERT_NON_NULL(img, "ImageHandle did not resolve to a physical image");
    return {img->width(), img->height()};
  }

  void BindGraphicsPipeline(
      const ir::GraphicsPipelineDescriptor& descriptor) override {
    ir::render_graph::ValidateGraphicsPipelineForPass(pass_, descriptor);
    if (descriptor.depth_test.has_value()) {
      ASSERT_TRUE(
          rendering_info_.depth_attachment_format != intl::Format::eUndefined,
          std::format("Pipeline '{}' enables depth test in pass '{}' "
                      "whose depth/stencil attachment has no depth "
                      "aspect",
                      descriptor.pipeline_name, pass_.name));
    }
    if (descriptor.stencil_test.has_value()) {
      ASSERT_TRUE(
          rendering_info_.stencil_attachment_format != intl::Format::eUndefined,
          std::format("Pipeline '{}' enables stencil test in pass '{}' "
                      "whose depth/stencil attachment has no stencil "
                      "aspect",
                      descriptor.pipeline_name, pass_.name));
    }
    bound_pipeline_ = &pipeline_cache_.GetOrCreateGraphicsPipeline(
        descriptor, sample_count_, rendering_info_);
    bound_pipeline_->Bind(cmd_);
    if (descriptor.viewport_config.viewport.extent.x > 0.0f &&
        descriptor.viewport_config.viewport.extent.y > 0.0f) {
      const auto& vp = descriptor.viewport_config.viewport;
      const bool flip_y = descriptor.viewport_config.flip_y;
      const auto viewport =
          intl::Viewport{}
              .setX(vp.origin.x)
              .setY(flip_y ? vp.origin.y + vp.extent.y : vp.origin.y)
              .setWidth(vp.extent.x)
              .setHeight(flip_y ? -vp.extent.y : vp.extent.y)
              .setMinDepth(0.0f)
              .setMaxDepth(1.0f);
      cmd_.setViewport(0, viewport);
    }
    if (descriptor.viewport_config.scissor.extent.x > 0 &&
        descriptor.viewport_config.scissor.extent.y > 0) {
      const auto& sc = descriptor.viewport_config.scissor;
      cmd_.setScissor(0, intl::Rect2D{{sc.origin.x, sc.origin.y},
                                      {CAST_TO_UINT(sc.extent.x),
                                       CAST_TO_UINT(sc.extent.y)}});
    }
    ClearPendingDescriptors();
  }

  void BindComputePipeline(
      const ir::ComputePipelineDescriptor& descriptor) override {
    ASSERT_TRUE(pass_.type == ir::PassType::kCompute,
                "BindComputePipeline is only valid inside compute passes");
    bound_pipeline_ = &pipeline_cache_.GetOrCreateComputePipeline(descriptor);
    bound_pipeline_->Bind(cmd_);
    ClearPendingDescriptors();
  }

  void BindVertexBuffer(uint32_t binding_point, const ir::Buffer& buffer,
                        size_t offset) override {
    const auto& vk_buffer = static_cast<const Buffer&>(buffer);
    cmd_.bindVertexBuffers(binding_point, vk_buffer.vk_buffer(),
                           static_cast<intl::DeviceSize>(offset));
  }

  void BindIndexBuffer(const ir::Buffer& buffer, size_t offset,
                       bool is_uint16) override {
    const auto& vk_buffer = static_cast<const Buffer&>(buffer);
    cmd_.bindIndexBuffer(
        vk_buffer.vk_buffer(), static_cast<intl::DeviceSize>(offset),
        is_uint16 ? intl::IndexType::eUint16 : intl::IndexType::eUint32);
  }

  void BindBuffer(uint32_t binding, const ir::Buffer& buffer, size_t offset,
                  size_t range, uint32_t array_element) override {
    const intl::DeviceSize vk_range =
        range == std::numeric_limits<size_t>::max()
            ? VK_WHOLE_SIZE
            : static_cast<intl::DeviceSize>(range);
    pending_buffer_bindings_.push_back(Pipeline::BufferBinding{
        .binding = binding,
        .buffer = &static_cast<const Buffer&>(buffer),
        .offset = static_cast<intl::DeviceSize>(offset),
        .range = vk_range,
        .array_element = array_element,
    });
  }

  void BindBuffer(uint32_t binding, ir::BufferHandle buffer, size_t offset,
                  size_t range, uint32_t array_element) override {
    BindBuffer(binding, GetBuffer(buffer), offset, range, array_element);
  }

  void BindImage(uint32_t binding, ir::ImageHandle image,
                 const ir::SamplerDescriptor* sampler,
                 uint32_t array_element) override {
    const Image* vk_image = resolved_images_[image.id];
    ASSERT_NON_NULL(vk_image,
                    "ImageHandle did not resolve to a physical image");
    intl::ImageLayout layout = intl::ImageLayout::eShaderReadOnlyOptimal;
    for (const ir::ImageAccess& access : pass_.image_accesses) {
      if (access.handle.id == image.id) {
        layout = image::GetImageLayout(access.usage);
      }
    }
    pending_image_bindings_.push_back(Pipeline::ImageBinding{
        .binding = binding,
        .image = vk_image,
        .sampler =
            pipeline_cache_.GetOrCreateSampler(sampler, vk_image->mip_levels()),
        .layout = layout,
        .array_element = array_element,
    });
  }

  void BindExternalImage(uint32_t binding, const ir::Image& image,
                         const ir::SamplerDescriptor* sampler,
                         uint32_t array_element) override {
    const auto& vk_image = Image::Cast(image);
    pending_image_bindings_.push_back(Pipeline::ImageBinding{
        .binding = binding,
        .image = &vk_image,
        .sampler =
            pipeline_cache_.GetOrCreateSampler(sampler, vk_image.mip_levels()),
        .layout = intl::ImageLayout::eShaderReadOnlyOptimal,
        .array_element = array_element,
    });
  }

  void PushConstants(ir::shader_stage::ShaderStage stage, uint32_t offset,
                     uint32_t size, const void* data) override {
    ASSERT_NON_NULL(bound_pipeline_, "No pipeline bound before PushConstants");
    bound_pipeline_->PushConstants(cmd_, type::ConvertShaderStages(stage),
                                   offset, size, data);
  }

  void Draw(uint32_t vertex_count, uint32_t instance_count,
            uint32_t first_vertex, uint32_t first_instance) override {
    FlushDescriptors();
    cmd_.draw(vertex_count, instance_count, first_vertex, first_instance);
  }

  void DrawIndexed(uint32_t index_count, uint32_t instance_count,
                   uint32_t first_index, int32_t vertex_offset,
                   uint32_t first_instance) override {
    FlushDescriptors();
    cmd_.drawIndexed(index_count, instance_count, first_index, vertex_offset,
                     first_instance);
  }

  void Dispatch(uint32_t group_count_x, uint32_t group_count_y,
                uint32_t group_count_z) override {
    FlushDescriptors();
    cmd_.dispatch(group_count_x, group_count_y, group_count_z);
  }

 private:
  void ClearPendingDescriptors() {
    pending_buffer_bindings_.clear();
    pending_image_bindings_.clear();
  }

  void FlushDescriptors() {
    if (pending_buffer_bindings_.empty() && pending_image_bindings_.empty()) {
      return;
    }
    ASSERT_NON_NULL(bound_pipeline_,
                    "No pipeline bound before flushing descriptors");
    bound_pipeline_->PushDescriptors(cmd_, pending_buffer_bindings_,
                                     pending_image_bindings_);
    ClearPendingDescriptors();
  }

  PipelineCache& pipeline_cache_;
  const intl::CommandBuffer cmd_;
  const ir::PassNode& pass_;
  const std::span<const Image* const> resolved_images_;
  const std::span<const Buffer* const> resolved_buffers_;
  const intl::SampleCountFlagBits sample_count_;
  const Pipeline::RenderingInfo& rendering_info_;

  const Pipeline* bound_pipeline_ = nullptr;
  std::vector<Pipeline::BufferBinding> pending_buffer_bindings_;
  std::vector<Pipeline::ImageBinding> pending_image_bindings_;
};

void ExecuteGraphicsPass(intl::CommandBuffer cmd, const ir::PassNode& pass,
                         std::span<const Image* const> resolved_images,
                         const intl::Extent2D& render_extent,
                         VkPassContext* pass_ctx) {
  std::unordered_map<uint32_t, ir::ImageHandle> resolve_targets;
  for (const ir::ResolveAttachmentDesc& resolve : pass.resolve_attachments) {
    resolve_targets[resolve.source.id] = resolve.target;
  }

  std::vector<intl::RenderingAttachmentInfo> color_infos;
  color_infos.reserve(pass.color_attachments.size());
  for (const ir::ColorAttachmentDesc& desc : pass.color_attachments) {
    const Image* color_img = resolved_images[desc.image.id];
    auto info = intl::RenderingAttachmentInfo{}
                    .setImageView(color_img->image_view())
                    .setImageLayout(intl::ImageLayout::eColorAttachmentOptimal)
                    .setLoadOp(type::ConvertAttachmentLoadOp(desc.load_op))
                    .setStoreOp(type::ConvertAttachmentStoreOp(desc.store_op))
                    .setClearValue(intl::ClearColorValue{std::array<float, 4>{
                        desc.clear_color.r, desc.clear_color.g,
                        desc.clear_color.b, desc.clear_color.a}});
    const auto resolve_iter = resolve_targets.find(desc.image.id);
    if (resolve_iter != resolve_targets.end()) {
      const Image* resolve_img = resolved_images[resolve_iter->second.id];
      info.setResolveMode(intl::ResolveModeFlagBits::eAverage)
          .setResolveImageView(resolve_img->image_view())
          .setResolveImageLayout(intl::ImageLayout::eColorAttachmentOptimal);
    }
    color_infos.push_back(info);
  }

  intl::RenderingAttachmentInfo depth_info;
  intl::RenderingAttachmentInfo stencil_info;
  bool has_depth = false;
  bool has_stencil = false;
  if (pass.depth_stencil_attachment.has_value()) {
    const ir::DepthStencilAttachmentDesc& ds = *pass.depth_stencil_attachment;
    const Image* ds_img = resolved_images[ds.image.id];
    const intl::ImageLayout ds_layout =
        ds.read_only ? intl::ImageLayout::eDepthStencilReadOnlyOptimal
                     : intl::ImageLayout::eDepthStencilAttachmentOptimal;
    const auto aspect = ds_img->GetAspectFlags();
    has_depth = static_cast<bool>(aspect & intl::ImageAspectFlagBits::eDepth);
    has_stencil =
        static_cast<bool>(aspect & intl::ImageAspectFlagBits::eStencil);
    if (has_depth) {
      depth_info =
          intl::RenderingAttachmentInfo{}
              .setImageView(ds_img->image_view())
              .setImageLayout(ds_layout)
              .setLoadOp(type::ConvertAttachmentLoadOp(ds.depth_load_op))
              .setStoreOp(type::ConvertAttachmentStoreOp(ds.depth_store_op))
              .setClearValue(intl::ClearDepthStencilValue{ds.clear_depth,
                                                          ds.clear_stencil});
    }
    if (has_stencil) {
      stencil_info =
          intl::RenderingAttachmentInfo{}
              .setImageView(ds_img->image_view())
              .setImageLayout(ds_layout)
              .setLoadOp(type::ConvertAttachmentLoadOp(ds.stencil_load_op))
              .setStoreOp(type::ConvertAttachmentStoreOp(ds.stencil_store_op))
              .setClearValue(intl::ClearDepthStencilValue{ds.clear_depth,
                                                          ds.clear_stencil});
    }
  }

  const intl::Rect2D render_area{{0, 0}, render_extent};
  const auto rendering_info =
      intl::RenderingInfo{}
          .setRenderArea(render_area)
          .setLayerCount(1)
          .setColorAttachments(color_infos)
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
  cmd.setViewport(0, viewport);
  cmd.setScissor(0, render_area);

  if (pass.execute) {
    pass.execute(*pass_ctx);
  }
  cmd.endRendering();
}

}  // namespace

RenderGraphExecutor::RenderGraphExecutor(const SharedContext& context,
                                         int num_frames_in_flight)
    : WithSharedContext{context},
      transient_pool_{context, num_frames_in_flight},
      pipeline_cache_{context} {}

void RenderGraphExecutor::Execute(
    intl::CommandBuffer cmd, int frame_index, const ir::RenderGraph& graph,
    std::span<const Swapchain* const> swapchains,
    std::span<const uint32_t> acquired_swapchain_image_indices) {
  const ir::CompiledRenderGraph compiled = graph.Compile();
  const ShaderModule::AutoReleaseShaderPool shader_pool;
  transient_pool_.BeginFrame(frame_index);

  ExecutionResourceState resource_state;
  ResolveExternalResources(graph, swapchains, acquired_swapchain_image_indices,
                           &resource_state);
  AcquireTransientResources(*context_, graph, compiled, frame_index,
                            &transient_pool_, &resource_state);

  std::vector<GraphvizBarrierNote> barrier_notes;
  for (uint32_t pass_index : compiled.execution_order) {
    const ir::PassNode& pass = graph.passes().at(pass_index);
    RecordPassBarriers(cmd, pass, &resource_state, &barrier_notes);

    intl::SampleCountFlagBits sample_count = intl::SampleCountFlagBits::e1;
    intl::Extent2D render_extent{0, 0};
    const Pipeline::RenderingInfo rendering_info = BuildPassRenderingInfo(
        pass, resource_state.resolved_images, &sample_count, &render_extent);
    VkPassContext pass_ctx{pipeline_cache_,
                           cmd,
                           pass,
                           resource_state.resolved_images,
                           resource_state.resolved_buffers,
                           sample_count,
                           rendering_info};

    if (pass.type == ir::PassType::kCompute) {
      if (pass.execute) {
        pass.execute(pass_ctx);
      }
    } else {
      ExecuteGraphicsPass(cmd, pass, resource_state.resolved_images,
                          render_extent, &pass_ctx);
    }
  }

  RecordFinalizationBarriers(cmd, graph, resource_state, &barrier_notes);
  last_graphviz_ =
      ir::render_graph::ExportGraphviz(graph, compiled, barrier_notes);
}

}  // namespace lighter::renderer::vk
