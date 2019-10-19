//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/text_util.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// This is the base class of all text renderer classes. The user should use it
// through derived classes. It gathers common members and methods of renderers.
class Text {
 public:
  using Font = CharLoader::Font;

  // We only support the horizontal layout for now.
  enum class Align { kLeft, kCenter, kRight };

  // Rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  // This should be called after a renderer is constructed and whenever
  // framebuffers are resized.
  void Update(const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const RenderPass& render_pass, uint32_t subpass_index);

 protected:
  Text(SharedBasicContext context,
       std::string&& pipeline_name,
       int num_frames_in_flight);

  // Updates the color and alpha sent to the shader.
  void UpdateUniformBuffer(int frame, const glm::vec3& color, float alpha);

  // Pointer to context.
  const SharedBasicContext context_;

  // Vertex buffer for rendering bounding boxes of characters or texts.
  DynamicPerVertexBuffer vertex_buffer_;

  // Sends color and alpha to the shader.
  UniformBuffer uniform_buffer_;

  // Graphics pipeline.
  PipelineBuilder pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

// This class renders each elements of 'texts' to one texture, so that later
// when the user wants to render any of them, this renderer only needs to bind
// the corresponding texture. This is backed by TextLoader.
class StaticText : public Text {
 public:
  StaticText(SharedBasicContext context,
             int num_frames_in_flight,
             const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  StaticText(const StaticText&) = delete;
  StaticText& operator=(const StaticText&) = delete;

  // Renders text at 'text_index' and returns left and right boundary.
  // Every character will keep its original width height ratio.
  // 'height', 'base_x', 'base_y' and returned values are in range [0.0, 1.0].
  // This should be called when 'command_buffer' is recording commands.
  glm::vec2 Draw(const VkCommandBuffer& command_buffer,
                 int frame, const VkExtent2D& frame_size, int text_index,
                 const glm::vec3& color, float alpha, float height,
                 float base_x, float base_y, Align align);

 private:
  // Renders each text (containing multiple characters) to one texture.
  TextLoader text_loader_;

  // Descriptors indexed by frame.
  std::vector<std::unique_ptr<DynamicDescriptor>> descriptors_;

  // Descriptor updaters indexed by frame. Each of them prepares the descriptor
  // at the same index to render the text at 'text_index'.
  std::vector<std::function<void(const VkCommandBuffer& command_buffer,
                                 const VkPipelineLayout& pipeline_layout,
                                 int text_index)>> push_descriptors_;
};

// This class renders all characters in 'texts' to one texture, so that when the
// user wants to render any combination of those characters, this renderer only
// needs to bind that texture. This is backed by CharLoader.
class DynamicText : public Text {
 public:
  DynamicText(SharedBasicContext context,
              int num_frames_in_flight,
              const std::vector<std::string>& texts,
              Font font, int font_height);

  // This class is neither copyable nor movable.
  DynamicText(const DynamicText&) = delete;
  DynamicText& operator=(const DynamicText&) = delete;

  // Renders 'text' and returns left and right boundary. Each character of
  // 'text' must have been included in 'texts' passed to the constructor.
  // Every character will keep its original width height ratio.
  // 'height', 'base_x', 'base_y' and returned values are in range [0.0, 1.0].
  // This should be called when 'command_buffer' is recording commands.
  glm::vec2 Draw(const VkCommandBuffer& command_buffer,
                 int frame, const VkExtent2D& frame_size,
                 const std::string& text, const glm::vec3& color, float alpha,
                 float height, float base_x, float base_y, Align align);

 private:
  // Renders all characters that may be used onto one big texture, so that we
  // only need to bind that texture to render different combinations of chars.
  CharLoader char_loader_;

  // Descriptors indexed by frame.
  std::vector<std::unique_ptr<StaticDescriptor>> descriptors_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H */
