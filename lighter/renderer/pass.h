//
//  pass.h
//
//  Created by Pujun Lun on 10/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_PASS_H
#define LIGHTER_RENDERER_PASS_H

#include <functional>
#include <string>

#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/string_view.h"

namespace lighter {
namespace renderer {

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

class BasePassDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  BasePassDescriptor(BasePassDescriptor&&) noexcept = default;
  BasePassDescriptor(const BasePassDescriptor&) = default;

  virtual ~BasePassDescriptor() = default;

 protected:
  using ImageUsageHistoryMap = absl::flat_hash_map<std::string,
                                                   ImageUsageHistory>;

  explicit BasePassDescriptor(int num_subpasses)
      : num_subpasses_{num_subpasses} {}

  // Adds an image that is used in this pass. This also checks whether subpasses
  // stored in 'usage_history' are out of range.
  void AddImage(absl::string_view name, ImageUsageHistory&& usage_history);

  // Checks whether 'subpass' is in range:
  //   - [0, 'num_subpasses_'), if 'include_virtual_subpasses' is false.
  //   - [virtual_initial_subpass_index(), virtual_final_subpass_index()], if
  //     'include_virtual_subpasses' is true.
  void ValidateSubpass(int subpass, absl::string_view image_name,
                       bool include_virtual_subpasses) const;

  // Images are in their initial/final layouts at the virtual subpasses.
  int virtual_initial_subpass_index() const { return -1; }
  int virtual_final_subpass_index() const { return num_subpasses_; }

  // Returns whether 'subpass' is a virtual subpass.
  bool IsVirtualSubpass(int subpass) const {
    return subpass == virtual_initial_subpass_index() ||
           subpass == virtual_final_subpass_index();
  }

  // Accessors.
  int num_subpasses() const { return num_subpasses_; }
  const ImageUsageHistoryMap& image_usage_history_map() const {
    return image_usage_history_map_;
  }

 private:
  // Number of subpasses.
  const int num_subpasses_;

  // Maps image name to usage history.
  ImageUsageHistoryMap image_usage_history_map_;
};

class GraphicsPassDescriptor : public BasePassDescriptor {
 public:
  // Returns the location attribute value of a color attachment at 'subpass'.
  using GetLocation = std::function<int(int subpass)>;

  struct LoadStoreOps {
    AttachmentLoadOp load_op;
    AttachmentStoreOp store_op;
  };

  using ColorLoadStoreOps = LoadStoreOps;

  struct DepthStencilLoadStoreOps {
    LoadStoreOps depth_ops;
    LoadStoreOps stencil_ops;
  };

  // This class provides copy constructor and move constructor.
  GraphicsPassDescriptor(GraphicsPassDescriptor&&) noexcept = default;
  GraphicsPassDescriptor(const GraphicsPassDescriptor&) = default;

  // Adds attachments used in this graphics pass.
  virtual GraphicsPassDescriptor& AddColorAttachment(
      absl::string_view name, ImageUsageHistory&& usage_history,
      const ColorLoadStoreOps& load_store_ops, GetLocation&& get_location) = 0;
  virtual GraphicsPassDescriptor& AddDepthStencilAttachment(
      absl::string_view name, ImageUsageHistory&& usage_history,
      const DepthStencilLoadStoreOps& load_store_ops) = 0;

 protected:
  enum class AttachmentType{ kColor, kDepthStencil };

  // Checks that all usages are expected given "attachment_type".
  void ValidateUsages(absl::string_view image_name,
                      const ImageUsageHistory& usage_history,
                      AttachmentType attachment_type) const;
};

class ComputePassDescriptor : public BasePassDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  ComputePassDescriptor(ComputePassDescriptor&&) noexcept = default;
  ComputePassDescriptor(const ComputePassDescriptor&) = default;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_PASS_H */
