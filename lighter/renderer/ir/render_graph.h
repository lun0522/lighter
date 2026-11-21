//
//  render_graph.h
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_RENDER_GRAPH_H
#define LIGHTER_RENDERER_IR_RENDER_GRAPH_H

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/base/util.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::ir {

inline constexpr uint32_t kInvalidResourceId =
    std::numeric_limits<uint32_t>::max();

// Versioned virtual image handle within a RenderGraph.
struct ImageHandle {
  uint32_t id = kInvalidResourceId;
  uint32_t version = 0;

  bool IsValid() const { return id != kInvalidResourceId; }
  bool operator==(const ImageHandle&) const = default;
};

// Versioned virtual buffer handle within a RenderGraph.
struct BufferHandle {
  uint32_t id = kInvalidResourceId;
  uint32_t version = 0;

  bool IsValid() const { return id != kInvalidResourceId; }
  bool operator==(const BufferHandle&) const = default;
};

// Format categories for RenderGraph-managed transient images.
enum class TransientImageFormat {
  kColorUnorm8,
  kColorFloat16,
  kDepthStencil,
  kMatchExternalImage,
};

// Descriptor for a transient image allocated and pooled by the RenderGraph.
struct TransientImageDesc {
  std::string name;
  glm::ivec2 extent{0, 0};
  TransientImageFormat format = TransientImageFormat::kColorUnorm8;
  MultisamplingMode multisampling_mode = MultisamplingMode::kNone;
  // Optional reference image used when format == kMatchExternalImage or when
  // extent is {0, 0} to inherit extent from the reference image.
  ImageHandle match_image;
};

// Descriptor for a transient buffer managed by the RenderGraph.
struct TransientBufferDesc {
  std::string name;
  size_t size = 0;
};

// Image read or write declared by a pass.
struct ImageAccess {
  ImageHandle handle;
  ImageUsage usage;
  bool is_write = false;
};

// Buffer read or write declared by a pass.
struct BufferAccess {
  BufferHandle handle;
  BufferUsage usage;
  bool is_write = false;
};

// Color attachment configuration for a graphics pass.
struct ColorAttachmentDesc {
  ImageHandle image;
  int location = 0;
  AttachmentLoadOp load_op = AttachmentLoadOp::kClear;
  AttachmentStoreOp store_op = AttachmentStoreOp::kStore;
  glm::vec4 clear_color{0.0f, 0.0f, 0.0f, 1.0f};
};

// MSAA resolve pair within a graphics pass.
struct ResolveAttachmentDesc {
  ImageHandle source;
  ImageHandle target;
};

// Depth/stencil attachment configuration for a graphics pass.
struct DepthStencilAttachmentDesc {
  ImageHandle image;
  AttachmentLoadOp depth_load_op = AttachmentLoadOp::kClear;
  AttachmentStoreOp depth_store_op = AttachmentStoreOp::kDontCare;
  AttachmentLoadOp stencil_load_op = AttachmentLoadOp::kDontCare;
  AttachmentStoreOp stencil_store_op = AttachmentStoreOp::kDontCare;
  float clear_depth = 1.0f;
  uint32_t clear_stencil = 0;
  bool read_only = false;
};

enum class PassType { kGraphics, kCompute };

// Execution-time interface provided to each pass's execute callback to resolve
// virtual handles and record graphics or compute commands.
class PassContext {
 public:
  PassContext(const PassContext&) = delete;
  PassContext& operator=(const PassContext&) = delete;

  virtual ~PassContext() = default;

  // Resolves virtual handles to backing physical resources for this frame.
  virtual const Image& GetImage(ImageHandle handle) const = 0;
  virtual const Buffer& GetBuffer(BufferHandle handle) const = 0;
  virtual glm::ivec2 GetImageExtent(ImageHandle handle) const = 0;

  // Pipeline & resource bindings.
  virtual void BindGraphicsPipeline(
      const GraphicsPipelineDescriptor& descriptor) = 0;
  virtual void BindComputePipeline(
      const ComputePipelineDescriptor& descriptor) = 0;
  virtual void BindVertexBuffer(uint32_t binding_point, const Buffer& buffer,
                                size_t offset = 0) = 0;
  virtual void BindIndexBuffer(const Buffer& buffer, size_t offset = 0,
                               bool is_uint16 = false) = 0;
  virtual void BindBuffer(uint32_t binding, const Buffer& buffer,
                          size_t offset = 0,
                          size_t range = std::numeric_limits<size_t>::max(),
                          uint32_t array_element = 0) = 0;
  virtual void BindBuffer(uint32_t binding, BufferHandle buffer,
                          size_t offset = 0,
                          size_t range = std::numeric_limits<size_t>::max(),
                          uint32_t array_element = 0) = 0;
  virtual void BindImage(uint32_t binding, ImageHandle image,
                         const SamplerDescriptor* sampler = nullptr,
                         uint32_t array_element = 0) = 0;
  virtual void BindExternalImage(uint32_t binding, const Image& image,
                                 const SamplerDescriptor* sampler = nullptr,
                                 uint32_t array_element = 0) = 0;
  virtual void PushConstants(shader_stage::ShaderStage stage, uint32_t offset,
                             uint32_t size, const void* data) = 0;

  // Draw & dispatch commands.
  virtual void Draw(uint32_t vertex_count, uint32_t instance_count = 1,
                    uint32_t first_vertex = 0, uint32_t first_instance = 0) = 0;
  virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1,
                           uint32_t first_index = 0, int32_t vertex_offset = 0,
                           uint32_t first_instance = 0) = 0;
  virtual void Dispatch(uint32_t group_count_x, uint32_t group_count_y = 1,
                        uint32_t group_count_z = 1) = 0;

 protected:
  PassContext() = default;
};

class RenderGraph;

// Declarative pass setup builder used inside AddGraphicsPass / AddComputePass
// setup callbacks to declare resource creations, reads, writes, and outputs.
class PassBuilder {
 public:
  PassBuilder(const PassBuilder&) = delete;
  PassBuilder& operator=(const PassBuilder&) = delete;

  // Creates transient resources owned by the RenderGraph.
  ImageHandle CreateImage(const TransientImageDesc& desc);
  BufferHandle CreateBuffer(const TransientBufferDesc& desc);

  // Declares shader/transfer reads and writes on images and buffers.
  ImageHandle Read(ImageHandle image, const ImageUsage& usage);
  ImageHandle Write(ImageHandle image, const ImageUsage& usage);
  BufferHandle Read(BufferHandle buffer, const BufferUsage& usage);
  BufferHandle Write(BufferHandle buffer, const BufferUsage& usage);

  // Declares render-target attachments (valid only for kGraphics passes).
  ImageHandle ColorAttachment(
      ImageHandle image, int location = 0,
      AttachmentLoadOp load_op = AttachmentLoadOp::kClear,
      AttachmentStoreOp store_op = AttachmentStoreOp::kStore,
      const glm::vec4& clear_color = {0.0f, 0.0f, 0.0f, 1.0f});

  ImageHandle DepthAttachment(
      ImageHandle image,
      AttachmentLoadOp depth_load_op = AttachmentLoadOp::kClear,
      AttachmentStoreOp depth_store_op = AttachmentStoreOp::kDontCare,
      AttachmentLoadOp stencil_load_op = AttachmentLoadOp::kDontCare,
      AttachmentStoreOp stencil_store_op = AttachmentStoreOp::kDontCare,
      float clear_depth = 1.0f, uint32_t clear_stencil = 0,
      bool read_only = false);

  ImageHandle ResolveAttachment(ImageHandle source, ImageHandle target);

  // Prevents unused-pass culling even if no downstream handle is consumed.
  PassBuilder& SetSideEffect(bool has_side_effect = true);

 private:
  friend class RenderGraph;

  PassBuilder(RenderGraph& graph, uint32_t pass_index, PassType pass_type);

  RenderGraph& graph_;
  const uint32_t pass_index_;
  const PassType pass_type_;
};

// Graph-level record for one virtual image resource.
struct ImageResourceNode {
  uint32_t id = kInvalidResourceId;
  std::string name;
  bool is_external = false;
  int swapchain_window_index = -1;
  const Image* external_image = nullptr;
  std::optional<TransientImageDesc> transient_desc;
  ImageUsage initial_usage;
  ImageUsage final_usage;
  std::vector<ImageUsage> all_usages;
  uint32_t current_version = 0;
  // Pass index that produced each version (version_writers[v] wrote version v;
  // version 0 for imported resources has writer -1).
  std::vector<int> version_writers;

  bool is_swapchain() const { return swapchain_window_index >= 0; }
};

// Graph-level record for one virtual buffer resource.
struct BufferResourceNode {
  uint32_t id = kInvalidResourceId;
  std::string name;
  bool is_external = false;
  const Buffer* external_buffer = nullptr;
  std::optional<TransientBufferDesc> transient_desc;
  BufferUsage initial_usage;
  BufferUsage final_usage;
  std::vector<BufferUsage> all_usages;
  uint32_t current_version = 0;
  std::vector<int> version_writers;
};

// Declarative pass node stored in a RenderGraph.
struct PassNode {
  using ExecuteCallback = std::function<void(PassContext&)>;

  uint32_t index = 0;
  std::string name;
  PassType type = PassType::kGraphics;
  std::vector<ImageAccess> image_accesses;
  std::vector<BufferAccess> buffer_accesses;
  std::vector<ColorAttachmentDesc> color_attachments;
  std::vector<ResolveAttachmentDesc> resolve_attachments;
  std::optional<DepthStencilAttachmentDesc> depth_stencil_attachment;
  bool has_side_effect = false;
  ExecuteCallback execute;
};

// Lifetime range [first_pass, last_pass] measured in execution_order indices
// (-1 if the resource is never touched by any live pass).
struct ResourceLifetime {
  int first_pass = -1;
  int last_pass = -1;

  bool IsUsed() const { return first_pass >= 0; }
};

// Output of RenderGraph::Compile(), consumed by the backend executor.
struct CompiledRenderGraph {
  std::vector<uint32_t> execution_order;
  std::vector<bool> pass_culled;
  std::vector<ResourceLifetime> image_lifetimes;
  std::vector<ResourceLifetime> buffer_lifetimes;
};

// Declarative Frame Graph IR holding virtual resources and pass nodes.
class RenderGraph {
 public:
  using SetupCallback = std::function<void(PassBuilder&)>;
  using ExecuteCallback = PassNode::ExecuteCallback;

  RenderGraph() = default;

  RenderGraph(const RenderGraph&) = delete;
  RenderGraph& operator=(const RenderGraph&) = delete;
  RenderGraph(RenderGraph&&) noexcept = default;
  RenderGraph& operator=(RenderGraph&&) noexcept = default;

  // Imports a persistent external image (e.g. loaded texture or history
  // buffer).
  ImageHandle ImportImage(std::string_view name, const Image& image,
                          const ImageUsage& initial_usage = ImageUsage{},
                          const ImageUsage& final_usage = ImageUsage{});

  // Imports the current acquired backbuffer for 'window_index', transitioning
  // it to presentation at the end of the graph.
  ImageHandle ImportSwapchainImage(int window_index = 0);

  // Imports a persistent external buffer.
  BufferHandle ImportBuffer(std::string_view name, const Buffer& buffer,
                            const BufferUsage& initial_usage = BufferUsage{},
                            const BufferUsage& final_usage = BufferUsage{});

  // Declares a graphics pass. 'setup' runs immediately to register accesses.
  RenderGraph& AddGraphicsPass(std::string_view name,
                               const SetupCallback& setup,
                               ExecuteCallback execute);

  // Declares a compute pass. 'setup' runs immediately to register accesses.
  RenderGraph& AddComputePass(std::string_view name, const SetupCallback& setup,
                              ExecuteCallback execute);

  // Clears all passes and virtual resources for rebuilding the next frame.
  void Reset();

  // Compiles the graph: identifies live passes reachable from external writes
  // or side-effect passes, culls dead passes, topologically sorts live passes,
  // and computes resource lifetime intervals in execution order.
  CompiledRenderGraph Compile() const;

  // Accessors.
  const std::vector<PassNode>& passes() const { return passes_; }
  const std::vector<ImageResourceNode>& images() const { return images_; }
  const std::vector<BufferResourceNode>& buffers() const { return buffers_; }
  const ImageResourceNode& GetImageResource(ImageHandle handle) const;
  const BufferResourceNode& GetBufferResource(BufferHandle handle) const;

 private:
  friend class PassBuilder;

  RenderGraph& AddPass(std::string_view name, PassType type,
                       const SetupCallback& setup, ExecuteCallback execute);

  ImageHandle CreateTransientImage(const TransientImageDesc& desc);
  BufferHandle CreateTransientBuffer(const TransientBufferDesc& desc);
  ImageHandle RecordImageAccess(uint32_t pass_index, ImageHandle handle,
                                const ImageUsage& usage, bool is_write);
  BufferHandle RecordBufferAccess(uint32_t pass_index, BufferHandle handle,
                                  const BufferUsage& usage, bool is_write);

  std::vector<PassNode> passes_;
  std::vector<ImageResourceNode> images_;
  std::vector<BufferResourceNode> buffers_;
};

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_RENDER_GRAPH_H
