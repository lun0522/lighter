//
//  pass.h
//
//  Created by Pujun Lun on 10/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_PASS_H
#define LIGHTER_RENDERER_IR_PASS_H

#include <optional>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/type.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter::renderer::ir {

struct SubpassDescriptor {
  struct MultisampleResolve {
    MultisampleResolve(const Image* source_image,
                       const Image* target_image)
        : source_image{FATAL_IF_NULL(source_image)},
          target_image{FATAL_IF_NULL(target_image)} {}

    const Image* source_image;
    const Image* target_image;
  };

  SubpassDescriptor& AddPipeline(GraphicsPipelineDescriptor&& descriptor) {
    pipeline_descriptors.push_back(std::move(descriptor));
    return *this;
  }
  SubpassDescriptor& AddColorAttachment(const Image* attachment) {
    color_attachments.push_back(attachment);
    return *this;
  }
  SubpassDescriptor& AddMultisampleResolve(const Image* source_image,
                                           const Image* target_image) {
    multisample_resolves.push_back({source_image, target_image});
    return *this;
  }
  SubpassDescriptor& SetDepthStencilAttachment(const Image* attachment) {
    depth_stencil_attachment = attachment;
    return *this;
  }

  std::vector<GraphicsPipelineDescriptor> pipeline_descriptors;
  std::vector<const Image*> color_attachments;
  std::vector<MultisampleResolve> multisample_resolves;
  const Image* depth_stencil_attachment = nullptr;
};

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

  struct SubpassDependency {
    int from;
    int to;
    std::vector<const Image*> attachments;
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
  RenderPassDescriptor& AddSubpass(SubpassDescriptor&& descriptor) {
    subpass_descriptors.push_back(std::move(descriptor));
    return *this;
  }
  RenderPassDescriptor& AddSubpassDependency(SubpassDependency&& dependency) {
    subpass_dependencies.push_back(std::move(dependency));
    return *this;
  }

  absl::flat_hash_map<const Image*, ColorLoadStoreOps> color_ops_map;
  absl::flat_hash_map<const Image*, DepthStencilLoadStoreOps>
      depth_stencil_ops_map;
  std::vector<SubpassDescriptor> subpass_descriptors;
  std::vector<SubpassDependency> subpass_dependencies;
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
