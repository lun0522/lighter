//
//  image_viewer.h
//
//  Created by Pujun Lun on 8/4/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_IMAGE_VIEWER_H
#define LIGHTER_APPLICATION_VULKAN_IMAGE_VIEWER_H

#include <memory>

#include "lighter/application/vulkan/util.h"

namespace lighter {
namespace application {
namespace vulkan {

// This class is used for rendering the given image to full screen. It assume
// that the layout of image has been transitioned to be optimal for being
// sampled in the fragment shader. It is mainly used for debugging.
class ImageViewer {
 public:
  ImageViewer(const renderer::vulkan::SharedBasicContext& context,
              const renderer::vulkan::SamplableImage& image,
              int num_channels, bool flip_y);

  // This class is neither copyable nor movable.
  ImageViewer(const ImageViewer&) = delete;
  ImageViewer& operator=(const ImageViewer&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(const VkExtent2D& frame_size,
                         const renderer::vulkan::RenderPass& render_pass,
                         uint32_t subpass_index);

  // Renders the image.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer) const;

 private:
  // Objects used for rendering.
  std::unique_ptr<renderer::vulkan::StaticDescriptor> descriptor_;
  std::unique_ptr<renderer::vulkan::PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<renderer::vulkan::GraphicsPipelineBuilder> pipeline_builder_;
  std::unique_ptr<renderer::vulkan::Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_IMAGE_VIEWER_H */
