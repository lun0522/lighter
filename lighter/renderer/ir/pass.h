//
//  pass.h
//
//  Created by Pujun Lun on 10/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_PASS_H
#define LIGHTER_RENDERER_IR_PASS_H

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::ir {

struct RenderPassDescriptor {
  struct LoadStoreOps {
    AttachmentLoadOp load_op = AttachmentLoadOp::kDontCare;
    AttachmentStoreOp store_op = AttachmentStoreOp::kDontCare;
  };

  struct ColorLoadStoreOps : public LoadStoreOps {};

  struct DepthStencilLoadStoreOps {
    LoadStoreOps depth_ops;
    LoadStoreOps stencil_ops;
  };

  struct MultisampleResolve {
    MultisampleResolve(const Image* source_image, const Image* target_image)
        : source_image{FATAL_IF_NULL(source_image)},
          target_image{FATAL_IF_NULL(target_image)} {}

    const Image* source_image;
    const Image* target_image;
  };

  RenderPassDescriptor& AddAttachment(const Image* attachment,
                                      const ColorLoadStoreOps& ops) {
    color_ops_map.insert({attachment, ops});
    return *this;
  }
  RenderPassDescriptor& AddAttachment(const Image* attachment,
                                      const DepthStencilLoadStoreOps& ops) {
    depth_stencil_ops_map.insert({attachment, ops});
    return *this;
  }
  RenderPassDescriptor& AddPipeline(GraphicsPipelineDescriptor&& descriptor) {
    pipeline_descriptors.push_back(std::move(descriptor));
    return *this;
  }
  RenderPassDescriptor& AddColorAttachment(const Image* attachment) {
    color_attachments.push_back(attachment);
    return *this;
  }
  RenderPassDescriptor& AddMultisampleResolve(const Image* source_image,
                                              const Image* target_image) {
    multisample_resolves.push_back({source_image, target_image});
    return *this;
  }
  RenderPassDescriptor& SetDepthStencilAttachment(const Image* attachment) {
    depth_stencil_attachment = attachment;
    return *this;
  }

  std::unordered_map<const Image*, ColorLoadStoreOps> color_ops_map;
  std::unordered_map<const Image*, DepthStencilLoadStoreOps>
      depth_stencil_ops_map;
  std::vector<GraphicsPipelineDescriptor> pipeline_descriptors;
  std::vector<const Image*> color_attachments;
  std::vector<MultisampleResolve> multisample_resolves;
  const Image* depth_stencil_attachment = nullptr;
};

struct ComputePassDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  ComputePassDescriptor(ComputePassDescriptor&&) noexcept = default;
  ComputePassDescriptor(const ComputePassDescriptor&) = default;
};

class RenderPass {
 public:
  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  virtual ~RenderPass() = default;

 protected:
  explicit RenderPass() = default;
};

// Records draw commands inside an active graphics pass.
class CommandRecorder {
 public:
  CommandRecorder(const CommandRecorder&) = delete;
  CommandRecorder& operator=(const CommandRecorder&) = delete;

  virtual ~CommandRecorder() = default;

  virtual void BindPipeline(int pipeline_index) = 0;
  virtual void BindVertexBuffer(uint32_t binding_point, const Buffer& buffer,
                                size_t offset = 0) = 0;
  virtual void PushConstants(shader_stage::ShaderStage stage, uint32_t offset,
                             uint32_t size, const void* data) = 0;
  virtual void Draw(uint32_t vertex_count, uint32_t instance_count = 1,
                    uint32_t first_vertex = 0, uint32_t first_instance = 0) = 0;

 protected:
  CommandRecorder() = default;
};

class ComputePass {
 public:
  // This class is neither copyable nor movable.
  ComputePass(const ComputePass&) = delete;
  ComputePass& operator=(const ComputePass&) = delete;

  virtual ~ComputePass() = default;

 protected:
  explicit ComputePass() = default;
};

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_PASS_H
