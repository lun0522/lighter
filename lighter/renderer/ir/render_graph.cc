//
//  render_graph.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/ir/render_graph.h"

#include <algorithm>
#include <format>
#include <utility>

namespace lighter::renderer::ir {
namespace {

constexpr int kExternalProducerPass = -1;

void ValidateMatchImage(std::span<const ImageResourceNode> images,
                        const TransientImageDesc& desc) {
  if (desc.format == TransientImageFormat::kMatchExternalImage) {
    ASSERT_TRUE(
        desc.match_image.IsValid() && desc.match_image.id < images.size(),
        std::format("Transient image '{}' with kMatchExternalImage "
                    "must specify a valid match_image",
                    desc.name));
  }
  if (desc.extent.x <= 0 || desc.extent.y <= 0) {
    ASSERT_TRUE(
        desc.match_image.IsValid() && desc.match_image.id < images.size(),
        std::format("Transient image '{}' with non-positive extent "
                    "must specify a valid match_image",
                    desc.name));
  }
}

}  // namespace

PassBuilder::PassBuilder(RenderGraph& graph, uint32_t pass_index,
                         PassType pass_type)
    : graph_{graph}, pass_index_{pass_index}, pass_type_{pass_type} {}

ImageHandle PassBuilder::CreateImage(const TransientImageDesc& desc) {
  return graph_.CreateTransientImage(desc);
}

BufferHandle PassBuilder::CreateBuffer(const TransientBufferDesc& desc) {
  return graph_.CreateTransientBuffer(desc);
}

ImageHandle PassBuilder::Read(ImageHandle image, const ImageUsage& usage) {
  return graph_.RecordImageAccess(pass_index_, image, usage,
                                  /*is_write=*/false);
}

ImageHandle PassBuilder::Write(ImageHandle image, const ImageUsage& usage) {
  return graph_.RecordImageAccess(pass_index_, image, usage,
                                  /*is_write=*/true);
}

BufferHandle PassBuilder::Read(BufferHandle buffer, const BufferUsage& usage) {
  return graph_.RecordBufferAccess(pass_index_, buffer, usage,
                                   /*is_write=*/false);
}

BufferHandle PassBuilder::Write(BufferHandle buffer, const BufferUsage& usage) {
  return graph_.RecordBufferAccess(pass_index_, buffer, usage,
                                   /*is_write=*/true);
}

ImageHandle PassBuilder::ColorAttachment(ImageHandle image, int location,
                                         AttachmentLoadOp load_op,
                                         AttachmentStoreOp store_op,
                                         const glm::vec4& clear_color) {
  ASSERT_TRUE(pass_type_ == PassType::kGraphics,
              "ColorAttachment is only valid on graphics passes");
  const ImageHandle written = graph_.RecordImageAccess(
      pass_index_, image, ImageUsage::GetRenderTargetUsage(location),
      /*is_write=*/true);
  graph_.passes_[pass_index_].color_attachments.push_back(ColorAttachmentDesc{
      .image = written,
      .location = location,
      .load_op = load_op,
      .store_op = store_op,
      .clear_color = clear_color,
  });
  return written;
}

ImageHandle PassBuilder::DepthAttachment(
    ImageHandle image, AttachmentLoadOp depth_load_op,
    AttachmentStoreOp depth_store_op, AttachmentLoadOp stencil_load_op,
    AttachmentStoreOp stencil_store_op, float clear_depth,
    uint32_t clear_stencil, bool read_only) {
  ASSERT_TRUE(pass_type_ == PassType::kGraphics,
              "DepthAttachment is only valid on graphics passes");
  PassNode& pass = graph_.passes_[pass_index_];
  ASSERT_NO_VALUE(pass.depth_stencil_attachment,
                  "Pass already has a depth/stencil attachment");

  const AccessType access_type =
      read_only ? AccessType::kReadOnly : AccessType::kReadWrite;
  const ImageHandle result = graph_.RecordImageAccess(
      pass_index_, image, ImageUsage::GetDepthStencilUsage(access_type),
      /*is_write=*/!read_only);
  pass.depth_stencil_attachment = DepthStencilAttachmentDesc{
      .image = result,
      .depth_load_op = depth_load_op,
      .depth_store_op = depth_store_op,
      .stencil_load_op = stencil_load_op,
      .stencil_store_op = stencil_store_op,
      .clear_depth = clear_depth,
      .clear_stencil = clear_stencil,
      .read_only = read_only,
  };
  return result;
}

ImageHandle PassBuilder::ResolveAttachment(ImageHandle source,
                                           ImageHandle target) {
  ASSERT_TRUE(pass_type_ == PassType::kGraphics,
              "ResolveAttachment is only valid on graphics passes");
  const PassNode& pass = graph_.passes_[pass_index_];
  const bool has_source =
      std::any_of(pass.color_attachments.begin(), pass.color_attachments.end(),
                  [source](const ColorAttachmentDesc& desc) {
                    return desc.image.id == source.id;
                  });
  ASSERT_TRUE(
      has_source,
      "ResolveAttachment source must be a ColorAttachment in this pass");

  const ImageHandle written = graph_.RecordImageAccess(
      pass_index_, target, ImageUsage::GetMultisampleResolveTargetUsage(),
      /*is_write=*/true);
  graph_.passes_[pass_index_].resolve_attachments.push_back(
      ResolveAttachmentDesc{.source = source, .target = written});
  return written;
}

PassBuilder& PassBuilder::SetSideEffect(bool has_side_effect) {
  graph_.passes_[pass_index_].has_side_effect = has_side_effect;
  return *this;
}

ImageHandle RenderGraph::ImportImage(std::string_view name, const Image& image,
                                     const ImageUsage& initial_usage,
                                     const ImageUsage& final_usage) {
  const uint32_t id = static_cast<uint32_t>(images_.size());
  images_.push_back(ImageResourceNode{
      .id = id,
      .name = std::string{name},
      .is_external = true,
      .external_image = &image,
      .initial_usage = initial_usage,
      .final_usage = final_usage,
      .all_usages = {initial_usage},
      .current_version = 0,
      .version_writers = {kExternalProducerPass},
  });
  return ImageHandle{.id = id, .version = 0};
}

ImageHandle RenderGraph::ImportSwapchainImage(int window_index) {
  ASSERT_TRUE(window_index >= 0, "window_index must be non-negative");
  const uint32_t id = static_cast<uint32_t>(images_.size());
  const ImageUsage present_usage = ImageUsage::GetPresentationUsage();
  images_.push_back(ImageResourceNode{
      .id = id,
      .name = std::format("swapchain_{}", window_index),
      .is_external = true,
      .swapchain_window_index = window_index,
      .initial_usage = ImageUsage{},
      .final_usage = present_usage,
      .all_usages = {ImageUsage::GetRenderTargetUsage(
                         /*attachment_location=*/0),
                     ImageUsage::GetMultisampleResolveTargetUsage(),
                     present_usage},
      .current_version = 0,
      .version_writers = {kExternalProducerPass},
  });
  return ImageHandle{.id = id, .version = 0};
}

BufferHandle RenderGraph::ImportBuffer(std::string_view name,
                                       const Buffer& buffer,
                                       const BufferUsage& initial_usage,
                                       const BufferUsage& final_usage) {
  const uint32_t id = static_cast<uint32_t>(buffers_.size());
  buffers_.push_back(BufferResourceNode{
      .id = id,
      .name = std::string{name},
      .is_external = true,
      .external_buffer = &buffer,
      .initial_usage = initial_usage,
      .final_usage = final_usage,
      .all_usages = {initial_usage},
      .current_version = 0,
      .version_writers = {kExternalProducerPass},
  });
  return BufferHandle{.id = id, .version = 0};
}

RenderGraph& RenderGraph::AddGraphicsPass(std::string_view name,
                                          const SetupCallback& setup,
                                          ExecuteCallback execute) {
  return AddPass(name, PassType::kGraphics, setup, std::move(execute));
}

RenderGraph& RenderGraph::AddComputePass(std::string_view name,
                                         const SetupCallback& setup,
                                         ExecuteCallback execute) {
  return AddPass(name, PassType::kCompute, setup, std::move(execute));
}

RenderGraph& RenderGraph::AddPass(std::string_view name, PassType type,
                                  const SetupCallback& setup,
                                  ExecuteCallback execute) {
  const uint32_t pass_index = static_cast<uint32_t>(passes_.size());
  passes_.push_back(PassNode{
      .index = pass_index,
      .name = std::string{name},
      .type = type,
      .execute = std::move(execute),
  });
  PassBuilder builder{*this, pass_index, type};
  if (setup) {
    setup(builder);
  }
  return *this;
}

void RenderGraph::Reset() {
  passes_.clear();
  images_.clear();
  buffers_.clear();
}

const ImageResourceNode& RenderGraph::GetImageResource(
    ImageHandle handle) const {
  ASSERT_TRUE(handle.IsValid() && handle.id < images_.size(),
              std::format("Invalid ImageHandle id {}", handle.id));
  return images_[handle.id];
}

const BufferResourceNode& RenderGraph::GetBufferResource(
    BufferHandle handle) const {
  ASSERT_TRUE(handle.IsValid() && handle.id < buffers_.size(),
              std::format("Invalid BufferHandle id {}", handle.id));
  return buffers_[handle.id];
}

ImageHandle RenderGraph::CreateTransientImage(const TransientImageDesc& desc) {
  ValidateMatchImage(images_, desc);
  const uint32_t id = static_cast<uint32_t>(images_.size());
  images_.push_back(ImageResourceNode{
      .id = id,
      .name = desc.name,
      .is_external = false,
      .transient_desc = desc,
      .initial_usage = ImageUsage{},
      .final_usage = ImageUsage{},
      .current_version = 0,
      .version_writers = {kExternalProducerPass},
  });
  return ImageHandle{.id = id, .version = 0};
}

BufferHandle RenderGraph::CreateTransientBuffer(
    const TransientBufferDesc& desc) {
  ASSERT_TRUE(
      desc.size > 0,
      std::format("Transient buffer '{}' must have positive size", desc.name));
  const uint32_t id = static_cast<uint32_t>(buffers_.size());
  buffers_.push_back(BufferResourceNode{
      .id = id,
      .name = desc.name,
      .is_external = false,
      .transient_desc = desc,
      .initial_usage = BufferUsage{},
      .final_usage = BufferUsage{},
      .current_version = 0,
      .version_writers = {kExternalProducerPass},
  });
  return BufferHandle{.id = id, .version = 0};
}

ImageHandle RenderGraph::RecordImageAccess(uint32_t pass_index,
                                           ImageHandle handle,
                                           const ImageUsage& usage,
                                           bool is_write) {
  ASSERT_TRUE(handle.IsValid() && handle.id < images_.size(),
              std::format("Invalid ImageHandle id {}", handle.id));
  ImageResourceNode& node = images_[handle.id];
  ASSERT_TRUE(handle.version <= node.current_version,
              std::format("Image '{}' handle version {} out of range ({})",
                          node.name, handle.version, node.current_version));

  ImageHandle result = handle;
  if (is_write) {
    ASSERT_TRUE(
        handle.version == node.current_version,
        std::format("Stale write to image '{}' (version {} != current {})",
                    node.name, handle.version, node.current_version));
    ++node.current_version;
    node.version_writers.push_back(static_cast<int>(pass_index));
    result.version = node.current_version;
  }

  node.all_usages.push_back(usage);
  passes_[pass_index].image_accesses.push_back(ImageAccess{
      .handle = result,
      .usage = usage,
      .is_write = is_write,
  });
  return result;
}

BufferHandle RenderGraph::RecordBufferAccess(uint32_t pass_index,
                                             BufferHandle handle,
                                             const BufferUsage& usage,
                                             bool is_write) {
  ASSERT_TRUE(handle.IsValid() && handle.id < buffers_.size(),
              std::format("Invalid BufferHandle id {}", handle.id));
  BufferResourceNode& node = buffers_[handle.id];
  ASSERT_TRUE(handle.version <= node.current_version,
              std::format("Buffer '{}' handle version {} out of range ({})",
                          node.name, handle.version, node.current_version));

  BufferHandle result = handle;
  if (is_write) {
    ASSERT_TRUE(
        handle.version == node.current_version,
        std::format("Stale write to buffer '{}' (version {} != current {})",
                    node.name, handle.version, node.current_version));
    ++node.current_version;
    node.version_writers.push_back(static_cast<int>(pass_index));
    result.version = node.current_version;
  }

  node.all_usages.push_back(usage);
  passes_[pass_index].buffer_accesses.push_back(BufferAccess{
      .handle = result,
      .usage = usage,
      .is_write = is_write,
  });
  return result;
}

}  // namespace lighter::renderer::ir
