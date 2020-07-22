//
//  render_pass.h
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_RENDER_PASS_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_RENDER_PASS_H

#include <functional>
#include <memory>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/absl/types/variant.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Forward declarations.
class RenderPass;

// The user should use this class to create RenderPass. The internal states are
// preserved when it is used to build a render pass, so it can be reused later.
class RenderPassBuilder {
 public:
  // Returns the image to use when rendering to the framebuffer at
  // 'framebuffer_index'.
  using GetImage = std::function<const Image&(int framebuffer_index)>;

  // Information we need to describe an image attachment used in the render
  // pass, including what operations to perform when it is loaded or written to,
  // and the initial and final image layout. The image layout in each subpass
  // will be specified when we describe a subpass.
  struct Attachment {
    struct ColorOps {
      VkAttachmentLoadOp load_color_op;
      VkAttachmentStoreOp store_color_op;
    };

    struct DepthStencilOps {
      VkAttachmentLoadOp load_depth_op;
      VkAttachmentStoreOp store_depth_op;
      VkAttachmentLoadOp load_stencil_op;
      VkAttachmentStoreOp store_stencil_op;
    };

    absl::variant<ColorOps, DepthStencilOps> attachment_ops;
    VkImageLayout initial_layout;
    VkImageLayout final_layout;
  };

  // Information we need to describe attachments used in a subpass.
  // Note that 'multisampling_refs', if has value, must be of the same size as
  // 'color_refs', and each element of 'multisampling_refs' will resolve to the
  // attachment in 'color_refs' that has the same index.
  struct SubpassAttachments {
    std::vector<VkAttachmentReference> color_refs;
    absl::optional<std::vector<VkAttachmentReference>> multisampling_refs;
    absl::optional<VkAttachmentReference> depth_stencil_ref;
  };

  // Information we need to describe the dependency between the previous subpass
  // and the next subpass.
  struct SubpassDependency {
    struct SubpassInfo {
      // Index of the subpass. We may use 'kExternalSubpassIndex' to refer to
      // the subpass before (if used as 'prev_subpass.index') or after
      // (if 'next_subpass.index') this render pass.
      uint32_t index;

      // Which pipeline stage of the next subpass should wait for which stage
      // of the previous subpass. Frequently used options:
      //   - VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT: if we want to
      //       read/write to the color attachment.
      //   - VK_PIPELINE_STAGE_EARLY/LATE_FRAGMENT_TESTS_BIT: if we want to
      //       read/write to the depth stencil buffer.
      //   - VK_PIPELINE_STAGE_VERTEX/FRAGMENT_SHADER_BIT: if we only want to
      //       read (sample) the attachment.
      // This should always be non-zero.
      VkPipelineStageFlags stage_flags;

      // Which memory access of the next subpass should wait for which access
      // of the previous subpass. Frequently used options:
      //   - VK_ACCESS_SHADER_READ/WRITE_BIT: if we want to sample a texture or
      //       read/write to a buffer.
      //   - VK_ACCESS_COLOR/DEPTH_STENCIL_ATTACHMENT_READ/WRITE_BIT: if we want
      //       to read/write to an attachment.
      //   - VK_ACCESS_INPUT_ATTACHMENT_READ_BIT: if we use 'inputAttachment',
      //       in which case we also need to specify 'dependencyFlags'.
      // If the previous subpass does not write to the attachment (in which case
      // the attachment should be in the READ_ONLY layout), and the next will
      // write to it (should be in the ATTACHMENT layout), we can put a 0 here,
      // and the transition of layouts will insert a memory barrier implicitly.
      VkAccessFlags access_flags;
    };

    // TODO: Rename to src/dst subpass.
    SubpassInfo prev_subpass, next_subpass;
    VkDependencyFlags dependency_flags;
  };

  // Specifies which attachment will be resolved to the target attachment.
  // Note that 'multisample_reference' is the index of VkAttachmentReference,
  // while 'target_attachment' is the index of VkAttachmentDescription.
  struct MultisamplingPair {
    int multisample_reference;
    uint32_t target_attachment;
  };

  // Creates a list of VkAttachmentReference to describe the multisampling
  // relationships. The length of the list will be equal to 'num_color_refs'.
  static std::vector<VkAttachmentReference> CreateMultisamplingReferences(
      int num_color_refs, absl::Span<const MultisamplingPair> pairs);

  explicit RenderPassBuilder(SharedBasicContext context)
      : context_{std::move(FATAL_IF_NULL(context))} {}

  // This class is neither copyable nor movable.
  RenderPassBuilder(const RenderPassBuilder&) = delete;
  RenderPassBuilder& operator=(const RenderPassBuilder&) = delete;

  ~RenderPassBuilder() = default;

  // Sets the number of framebuffers.
  RenderPassBuilder& SetNumFramebuffers(int count);

  // Sets the image attachment at 'index'.
  RenderPassBuilder& SetAttachment(int index, const Attachment& attachment);

  // Informs the builder how to get the image for the attachment at 'index'.
  // SetAttachment() must have been called with this 'index'.
  // The user is responsible for keeping 'get_image' valid until Build().
  RenderPassBuilder& UpdateAttachmentImage(int index, GetImage&& get_image);

  // Sets the subpass at 'index' with color attachments and optional depth
  // stencil attachments used for this subpass.
  RenderPassBuilder& SetSubpass(
      int index, std::vector<VkAttachmentReference>&& color_refs,
      const absl::optional<VkAttachmentReference>& depth_stencil_ref);

  // Sets multisampling relationships for the subpass at 'subpass_index'.
  // The user must have called SetSubpass() for this subpass.
  RenderPassBuilder& SetMultisampling(
      int subpass_index,
      std::vector<VkAttachmentReference>&& multisampling_refs);

  // Creates a dependency relationship.
  RenderPassBuilder& AddSubpassDependency(const SubpassDependency& dependency);

  // Returns a render pass. This keeps internal states of the builder unchanged.
  // For simplicity, the size of framebuffers will be the same to the first
  // color attachment.
  std::unique_ptr<RenderPass> Build() const;

 private:
  // Pointer to context.
  const SharedBasicContext context_;

  // Number of framebuffers. It must have value when Build() is called.
  absl::optional<int> num_framebuffers_;

  // Descriptions of attachments used in this render pass.
  std::vector<VkAttachmentDescription> attachment_descriptions_;

  // Functions that return the image associated with each attachment.
  std::vector<GetImage> get_attachment_images_;

  // Values used to clear image attachments. They are used with
  // VK_ATTACHMENT_LOAD_OP_CLEAR.
  std::vector<VkClearValue> clear_values_;

  // Each element declares attachments used in the subpass with the same index.
  std::vector<SubpassAttachments> subpass_attachments_;

  // Dependency relationships between subpasses.
  std::vector<VkSubpassDependency> subpass_dependencies_;
};

// VkRenderPass gathers operations to perform when we render to one framebuffer.
// It consists of multiple subpasses, and configures the dependencies between
// subpasses and image attachments used in each subpass. With subpass
// dependencies, we can specify the rendering order if necessary. For example,
// we may want to render transparent objects after opaque objects. Another
// example is that for the deferred rendering, we need to access the previous
// rendering results, which must not happen before the previous writes to the
// framebuffer finish.
// The user should use RenderPassBuilder to create instances of this class. If
// the window is resized, the user should discard the old render pass and build
// a new one with the updated framebuffer size and image attachments.
class RenderPass {
 public:
  // Specifies rendering operations to perform in one subpass.
  using RenderOp = std::function<void(const VkCommandBuffer& command_buffer)>;

  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass();

  // Generates commands for rendering to the framebuffer at 'framebuffer_index'.
  // This should be called when 'command_buffer' is recording commands.
  // Each element of 'render_ops' represents the operations to perform in each
  // subpass, hence the size of 'render_ops' must be equal to 'num_subpasses_'.
  void Run(const VkCommandBuffer& command_buffer,
           int framebuffer_index, absl::Span<const RenderOp> render_ops) const;

  // Overloads.
  const VkRenderPass& operator*() const { return render_pass_; }

  // Accessors.
  int num_color_attachments(int subpass_index) const {
    return num_color_attachments_.at(subpass_index);
  }

 private:
  friend std::unique_ptr<RenderPass> RenderPassBuilder::Build() const;

  // 'clear_values' is intended to be passed by value.
  RenderPass(SharedBasicContext context,
             int num_subpasses,
             const VkRenderPass& render_pass,
             std::vector<VkClearValue> clear_values,
             const VkExtent2D& framebuffer_size,
             std::vector<VkFramebuffer>&& framebuffers,
             std::vector<int>&& num_color_attachments)
      : context_{std::move(FATAL_IF_NULL(context))},
        num_subpasses_{num_subpasses},
        render_pass_{render_pass},
        clear_values_{std::move(clear_values)},
        framebuffer_size_{framebuffer_size},
        framebuffers_{std::move(framebuffers)},
        num_color_attachments_{std::move(num_color_attachments)} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Number of subpasses.
  const int num_subpasses_;

  // Opaque render pass object.
  VkRenderPass render_pass_;

  // Values used to clear image attachments. They are used with
  // VK_ATTACHMENT_LOAD_OP_CLEAR.
  const std::vector<VkClearValue> clear_values_;

  // Size of each framebuffer.
  const VkExtent2D framebuffer_size_;

  // Framebuffers that are used as rendering targets.
  const std::vector<VkFramebuffer> framebuffers_;

  // Number of color attachments in the subpass with the same index.
  const std::vector<int> num_color_attachments_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_RENDER_PASS_H */
