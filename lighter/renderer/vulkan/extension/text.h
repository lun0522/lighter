//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_TEXT_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_TEXT_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/text_util.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/descriptor.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {
namespace vulkan {

// This is the base class of all text renderer classes. The user should use it
// through derived classes. It gathers common members of renderers.
// Update() must have been called before calling Draw() for the first time, and
// whenever the render pass is changed.
class Text {
 public:
  using Font = CharLoader::Font;

  // We only support the horizontal layout for now.
  enum class Align { kLeft, kCenter, kRight };

  // This class is neither copyable nor movable.
  Text(const Text&) = delete;
  Text& operator=(const Text&) = delete;

  virtual ~Text() = default;

  // Rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  // If 'flip_y' is true, point (0, 0) will be located at the upper left corner,
  // which is appropriate for presenting to the screen. The user can choose
  // whether or not to do the flipping for offscreen rendering.
  void Update(const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const RenderPass& render_pass, uint32_t subpass_index,
              bool flip_y);

  // Renders all texts tht have been added.
  // This should be called when 'command_buffer' is recording commands.
  virtual void Draw(const VkCommandBuffer& command_buffer,
                    int frame, const glm::vec3& color, float alpha) = 0;

 protected:
  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  Text(const SharedBasicContext& context,
       std::string&& pipeline_name,
       int num_frames_in_flight,
       float viewport_aspect_ratio);

  // Updates uniform buffer and vertex buffer, and returns the number of
  // meshes to render. 'vertices_to_draw_' will be cleared after calling this.
  int UpdateBuffers(int frame, const glm::vec3& color, float alpha);

  // Sets layout of graphics pipeline.
  void SetPipelineLayout(const VkDescriptorSetLayout& layout);

  // Returns descriptor info of uniform buffer at 'frame'.
  VkDescriptorBufferInfo GetUniformBufferDescriptorInfo(int frame) const;

  // Accessors.
  float viewport_aspect_ratio() const { return viewport_aspect_ratio_; }
  const PerVertexBuffer& vertex_buffer() const { return vertex_buffer_; }
  const Pipeline& pipeline() const {
    ASSERT_NON_NULL(pipeline_, "Update() must have been called");
    return *pipeline_;
  }
  std::vector<common::Vertex2D>* mutable_vertices() {
    return &vertices_to_draw_;
  }

 private:
  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // each character does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Vertices of added texts.
  std::vector<common::Vertex2D> vertices_to_draw_;

  // Vertex buffer for rendering bounding boxes of characters or texts.
  DynamicPerVertexBuffer vertex_buffer_;

  // Sends color and alpha to the shader.
  UniformBuffer uniform_buffer_;

  // Graphics pipeline.
  GraphicsPipelineBuilder pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

// This class renders each elements of 'texts' to one texture, so that later
// when the user wants to render any of them, this renderer only needs to bind
// the corresponding texture. This is backed by TextLoader.
class StaticText : public Text {
 public:
  StaticText(const SharedBasicContext& context,
             int num_frames_in_flight,
             float viewport_aspect_ratio,
             absl::Span<const std::string> texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  StaticText(const StaticText&) = delete;
  StaticText& operator=(const StaticText&) = delete;

  // Creates vertex data for rendering text at 'text_index', and returns left
  // and right boundary of the rendered text. 'base_x', 'base_y' and returned
  // values are in range [0.0, 1.0], while 'height' is in range [-1.0, 1.0].
  // Every character will keep its original aspect ratio. The vertex data will
  // be cleared after calling Draw(), hence the user should add all texts again
  // before the next call to Draw().
  glm::vec2 AddText(int text_index, float height, float base_x, float base_y,
                    Align align);

  // Overrides.
  void Draw(const VkCommandBuffer& command_buffer,
            int frame, const glm::vec3& color, float alpha) override;

 private:
  // Updates the descriptor for rendering the text at 'text_index'.
  // This should be called when 'command_buffer' is recording commands.
  void UpdateDescriptor(const VkCommandBuffer& command_buffer,
                        int frame, int text_index);

  // Renders each text (containing multiple characters) to one texture.
  TextLoader text_loader_;

  // Descriptors indexed by frame.
  std::vector<std::unique_ptr<DynamicDescriptor>> descriptors_;

  // Indices of texts to draw. This can contain duplicates.
  std::vector<int> texts_to_draw_;
};

// This class renders all characters in 'texts' to one texture, so that when the
// user wants to render any combination of those characters, this renderer only
// needs to bind that texture. This is backed by CharLoader.
class DynamicText : public Text {
 public:
  DynamicText(const SharedBasicContext& context,
              int num_frames_in_flight,
              float viewport_aspect_ratio,
              absl::Span<const std::string> texts,
              Font font, int font_height);

  // This class is neither copyable nor movable.
  DynamicText(const DynamicText&) = delete;
  DynamicText& operator=(const DynamicText&) = delete;

  // Creates vertex data for rendering 'text', and returns left and right
  // boundary of the rendered text. Each character must have been included in
  // 'texts' passed to the constructor. 'base_x', 'base_y' and returned values
  // are in range [0.0, 1.0], while 'height' is in range [-1.0, 1.0].
  // Every character will keep its original aspect ratio. The vertex data will
  // be cleared after calling Draw(), hence the user should add all texts again
  // before the next call to Draw().
  glm::vec2 AddText(const std::string& text, float height, float base_x,
                    float base_y, Align align);

  // Overrides.
  void Draw(const VkCommandBuffer& command_buffer,
            int frame, const glm::vec3& color, float alpha) override;

 private:
  // Renders all characters that may be used onto one big texture, so that we
  // only need to bind that texture to render different combinations of chars.
  CharLoader char_loader_;

  // Descriptors indexed by frame.
  std::vector<std::unique_ptr<StaticDescriptor>> descriptors_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_TEXT_H */
