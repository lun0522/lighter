//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_UI_TEXT_H
#define LIGHTER_UI_TEXT_H

#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "lighter/base/data.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/ui/text_util.h"

namespace lighter::ui {

// Base class for screen-space 2D text renderers executing inside a Frame Graph
// graphics pass. Uses per-frame-in-flight dynamic buffers rotated internally on
// each Draw() call.
class Text {
 public:
  using Font = CharLoader::Font;

  enum class Align { kLeft, kCenter, kRight };

  // This class is neither copyable nor movable.
  Text(const Text&) = delete;
  Text& operator=(const Text&) = delete;

  virtual ~Text() = default;

  // Renders all texts added since the previous Draw() call and clears the
  // queued vertex list.
  virtual void Draw(renderer::ir::PassContext& ctx,
                    const glm::ivec2& frame_size, const glm::vec3& color,
                    float alpha, bool flip_y = true) = 0;

 protected:
  static constexpr int kDefaultNumFramesInFlight = 2;

  Text(const renderer::ir::Renderer& renderer, std::string pipeline_name,
       float viewport_aspect_ratio,
       int num_frames_in_flight = kDefaultNumFramesInFlight);

  renderer::ir::GraphicsPipelineDescriptor BuildPipelineDescriptor(
      const glm::ivec2& frame_size, bool flip_y,
      int num_color_attachments = 1) const;

  // Uploads queued vertices and color/alpha uniform data for the current frame
  // slot, returning the number of quads to render and advancing the slot.
  struct PreparedFrameBuffers {
    int num_rects = 0;
    const renderer::ir::Buffer* uniform_buffer = nullptr;
    const renderer::ir::Buffer* quad_buffer = nullptr;
    size_t index_offset = 0;
  };
  PreparedFrameBuffers PrepareFrameBuffers(const glm::vec3& color, float alpha);

  float viewport_aspect_ratio() const { return viewport_aspect_ratio_; }
  std::vector<base::Vertex2D>* mutable_vertices() { return &vertices_to_draw_; }

 private:
  struct FrameBuffers {
    std::unique_ptr<renderer::ir::Buffer> uniform_buffer;
    std::unique_ptr<renderer::ir::Buffer> quad_buffer;
    int rect_capacity = 0;
  };

  const renderer::ir::Renderer& renderer_;
  const float viewport_aspect_ratio_;
  const renderer::ir::GraphicsPipelineDescriptor base_pipeline_descriptor_;

  std::vector<base::Vertex2D> vertices_to_draw_;
  std::vector<FrameBuffers> frame_buffers_;
  int current_frame_slot_ = 0;
};

// Renders static pre-baked strings via TextLoader.
class StaticText : public Text {
 public:
  StaticText(renderer::ir::Renderer& renderer, float viewport_aspect_ratio,
             std::span<const std::string> texts, Font font, int font_height,
             int num_frames_in_flight = kDefaultNumFramesInFlight);

  StaticText(const StaticText&) = delete;
  StaticText& operator=(const StaticText&) = delete;

  // Queues text at 'text_index' and returns its [left, right] X bounds in
  // [0.0, 1.0].
  glm::vec2 AddText(int text_index, float height, float base_x, float base_y,
                    Align align);

  void Draw(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
            const glm::vec3& color, float alpha, bool flip_y = true) override;

 private:
  TextLoader text_loader_;
  std::vector<int> texts_to_draw_;
};

// Renders arbitrary combinations of preloaded glyph characters from a single
// font atlas via CharLoader in one draw call.
class DynamicText : public Text {
 public:
  DynamicText(renderer::ir::Renderer& renderer, float viewport_aspect_ratio,
              std::span<const std::string> texts, Font font, int font_height,
              int num_frames_in_flight = kDefaultNumFramesInFlight);

  DynamicText(const DynamicText&) = delete;
  DynamicText& operator=(const DynamicText&) = delete;

  // Queues 'text' and returns its [left, right] X bounds in [0.0, 1.0].
  glm::vec2 AddText(const std::string& text, float height, float base_x,
                    float base_y, Align align);

  void Draw(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
            const glm::vec3& color, float alpha, bool flip_y = true) override;

 private:
  CharLoader char_loader_;
};

}  // namespace lighter::ui

#endif  // LIGHTER_UI_TEXT_H
