//
//  graphics_pass.h
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H

#include <functional>
#include <memory>
#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/base_pass.h"
#include "lighter/renderer/vulkan/extension/image_util.h"
#include "lighter/renderer/vulkan/wrapper/image_usage.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// This class is used to analyze usages of attachment images involved in a
// render pass, and create a render pass builder based on the analysis results.
// The internal states are preserved when it creates the render pass builder, so
// it can be reused later.
class GraphicsPass : public BasePass {
 public:
  using AttachmentLoadStoreOps = RenderPassBuilder::Attachment::LoadStoreOps;

  // Returns the location attribute value of a color attachment at 'subpass'.
  using GetLocation = std::function<int(int subpass)>;

  GraphicsPass(SharedBasicContext context, int num_subpasses);

  // This class is neither copyable nor movable.
  GraphicsPass(const GraphicsPass&) = delete;
  GraphicsPass& operator=(const GraphicsPass&) = delete;

  // Following functions return default load store ops. By default, the content
  // of the attachment will be cleared at the beginning of this graphics pass,
  // and will not be preserved after this graphics pass.
  static RenderPassBuilder::Attachment::ColorLoadStoreOps
  GetDefaultColorLoadStoreOps() {
    return {
        /*color_load_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
        /*color_store_op=*/VK_ATTACHMENT_STORE_OP_STORE,
    };
  }
  static RenderPassBuilder::Attachment::DepthStencilLoadStoreOps
  GetDefaultDepthStencilLoadStoreOps() {
    return {
        /*depth_load_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
        /*depth_store_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
        /*stencil_load_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
        /*stencil_store_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
    };
  }

  // Adds an image that is used in this graphics pass, and returns its index
  // within the VkAttachmentDescription array, which is used when calling
  // RenderPassBuilder::UpdateAttachmentImage(). Note that if the image is used
  // as render target at any subpass, 'get_location' must not be nullptr.
  // If 'load_store_ops' is not specified, default load store ops will be used.
  int AddAttachment(
      const std::string& image_name,
      GetLocation&& get_location, image::UsageHistory&& history,
      const absl::optional<AttachmentLoadStoreOps>& load_store_ops =
          absl::nullopt);

  // Specifies that the multisample source image will get resolved to the single
  // sample destination image at 'subpass'.
  GraphicsPass& AddMultisampleResolving(const std::string& src_image_name,
                                        const std::string& dst_image_name,
                                        int subpass);

  // Creates a render pass builder. This can be called multiple times. Note that
  // the user still need to call RenderPassBuilder::UpdateAttachmentImage() for
  // all images included in this graphics pass.
  std::unique_ptr<RenderPassBuilder> CreateRenderPassBuilder(
      int num_framebuffers);

 private:
  // Maps each multisample image to the single sample image that it will resolve
  // to. We should have such a map for each subpass.
  using MultisamplingMap = absl::flat_hash_map<std::string, std::string>;

  // Following functions populate 'render_pass_builder_'.
  void SetAttachments();
  void SetSubpasses();
  void SetSubpassDependencies();

  // Returns the first subpass where the image is used as render target, in
  // which case we need to query the location attribute value using the location
  // getter provided by the user. If there is no such subpass, returns
  // absl::nullopt instead.
  absl::optional<int> GetFirstSubpassRequiringLocationGetter(
      const image::UsageHistory& history) const;

  // Returns the usage type of an image. We assume that each image should either
  // always be a color attachment, or always be a depth stencil attachment
  // throughout all subpasses. Note that kMultisampleResolve is treated as
  // kRenderTarget. Hence, the return value can only be either kRenderTarget or
  // kDepthStencil.
  image::Usage::UsageType GetImageUsageTypeForAllSubpasses(
      const std::string& image_name, const image::UsageHistory& history) const;

  // Returns true if the image usage at 'subpass' is of 'usage_type'.
  bool CheckImageUsageType(const image::UsageHistory& history, int subpass,
                           image::Usage::UsageType usage_type) const;

  // Returns whether 'subpass' is a virtual subpass.
  bool IsVirtualSubpass(int subpass) const {
    return subpass == virtual_initial_subpass_index() ||
           subpass == virtual_final_subpass_index();
  }

  // Returns kExternalSubpassIndex if 'subpass' is a virtual subpass. Otherwise,
  // returns the casted input subpass.
  uint32_t RegulateSubpassIndex(int subpass) const {
    return IsVirtualSubpass(subpass) ? kExternalSubpassIndex
                                     : static_cast<uint32_t>(subpass);
  }

  // Overrides.
  void ValidateImageUsageHistory(
      const std::string& image_name,
      const image::UsageHistory& history) const override;

  // Pointer to context.
  const SharedBasicContext context_;

  // TODO: Use one map for all.
  // Maps attachment images to their indices within the VkAttachmentDescription
  // array.
  absl::flat_hash_map<std::string, int> attachment_index_map_;

  // Maps color attachments to their location attribute value getters.
  absl::flat_hash_map<std::string, GetLocation>
      color_attachment_location_getter_map_;

  // Maps attachment images to their load store ops.
  absl::flat_hash_map<std::string, AttachmentLoadStoreOps>
      attachment_load_store_ops_map_;

  // Each element maps multisample images to single sample images that they will
  // resolve to. Elements are indexed by subpass.
  std::vector<MultisamplingMap> multisampling_at_subpass_maps_;

  // Builder of RenderPass.
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H */
