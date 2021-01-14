//
//  pass.h
//
//  Created by Pujun Lun on 10/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_PASS_H
#define LIGHTER_RENDERER_PASS_H

#include <optional>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter::renderer {

class GraphicsOps {
 public:
  // This class is only movable.
  GraphicsOps(GraphicsOps&&) noexcept = default;
  GraphicsOps& operator=(GraphicsOps&&) noexcept = default;

  int AddPipeline(GraphicsPipelineDescriptor&& descriptor) {
    pipeline_descriptors_.push_back(std::move(descriptor));
    return (static_cast<int>(pipeline_descriptors_.size()) - 1) * 2;
  }

  int AddMultisamplingResolve(const DeviceImage* source_image,
                              const DeviceImage* target_image) {
    resolve_descriptors_.push_back({source_image, target_image});
    return (static_cast<int>(resolve_descriptors_.size()) - 1) * 2 + 1;
  }

 private:
  struct ResolveDescriptor {
    ResolveDescriptor(const DeviceImage* source_image,
                      const DeviceImage* target_image)
        : source_image{FATAL_IF_NULL(source_image)},
          target_image{FATAL_IF_NULL(target_image)} {}

    const DeviceImage* source_image;
    const DeviceImage* target_image;
  };

  std::vector<GraphicsPipelineDescriptor> pipeline_descriptors_;
  std::vector<ResolveDescriptor> resolve_descriptors_;
};

struct GraphicsPassDescriptor {
  struct LoadStoreOps {
    AttachmentLoadOp load_op;
    AttachmentStoreOp store_op;
  };

  using ColorLoadStoreOps = LoadStoreOps;

  struct DepthStencilLoadStoreOps {
    LoadStoreOps depth_ops;
    LoadStoreOps stencil_ops;
  };

  struct GraphicsOpDependency {
    int from_id;
    int to_id;
    std::vector<const DeviceImage*> attachments;
  };

  GraphicsPassDescriptor& SetLoadStoreOps(const DeviceImage* attachment,
                                          const ColorLoadStoreOps& ops) {
    color_ops_map.insert({attachment, ops});
    return *this;
  }
  GraphicsPassDescriptor& SetLoadStoreOps(const DeviceImage* attachment,
                                          const DepthStencilLoadStoreOps& ops) {
    depth_stencil_ops_map.insert({attachment, ops});
    return *this;
  }

  GraphicsPassDescriptor& SetGraphicsOps(GraphicsOps&& ops) {
    graphics_ops = std::move(ops);
    return *this;
  }
  GraphicsPassDescriptor& AddDependency(
      int from_id, int to_id, std::vector<const DeviceImage*>&& attachments) {
    dependencies.push_back({from_id, to_id, std::move(attachments)});
    return *this;
  }
  GraphicsPassDescriptor& AddDependency(int from_id, int to_id) {
    return AddDependency(from_id, to_id, {});
  }

  absl::flat_hash_map<const DeviceImage*, ColorLoadStoreOps> color_ops_map;
  absl::flat_hash_map<const DeviceImage*, DepthStencilLoadStoreOps>
      depth_stencil_ops_map;
  std::optional<GraphicsOps> graphics_ops;
  std::vector<GraphicsOpDependency> dependencies;
};

struct ComputePassDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  ComputePassDescriptor(ComputePassDescriptor&&) noexcept = default;
  ComputePassDescriptor(const ComputePassDescriptor&) = default;
};

class GraphicsPass {
 public:
  // This class is neither copyable nor movable.
  GraphicsPass(const GraphicsPass&) = delete;
  GraphicsPass& operator=(const GraphicsPass&) = delete;

  virtual ~GraphicsPass() = default;
};

class ComputePass {
 public:
  // This class is neither copyable nor movable.
  ComputePass(const ComputePass&) = delete;
  ComputePass& operator=(const ComputePass&) = delete;

  virtual ~ComputePass() = default;
};

}  // namespace lighter::renderer

#endif  // LIGHTER_RENDERER_PASS_H
