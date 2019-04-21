//
//  pipeline.h
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

/** VkPipeline stores the entire graphics pipeline.
 *
 *  Initialization:
 *    ShaderStage (vertex and fragment shaders)
 *    VertexInputState (how to interpret vertex attributes)
 *    InputAssemblyState (what topology to use)
 *    ViewportState (viewport and scissor)
 *    RasterizationState (lines, polygons, face culling, etc)
 *    MultisampleState (how many sample points)
 *    DepthStencilState
 *    ColorBlendState
 *    DynamicState (which properties of this pipeline will be dynamic)
 *    VkPipelineLayout (set uniform values)
 *    VkRenderPass and subpass
 *    BasePipeline (may copy settings from another pipeline)
 */
class Pipeline {
 public:
  using ShaderInfo = std::pair<std::string, VkShaderStageFlagBits>;

  Pipeline() = default;
  void Init(std::shared_ptr<Context> context,
            const std::vector<ShaderInfo>& shader_infos,
            const VkPipelineLayoutCreateInfo& layout_info,
            const VkPipelineVertexInputStateCreateInfo& vertex_input_info);
  void Cleanup();
  ~Pipeline() { Cleanup(); }

  // This class is neither copyable nor movable
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  const VkPipeline& operator*() const { return pipeline_; }
  const VkPipelineLayout& layout()  const { return pipeline_layout_; }

 private:
  std::shared_ptr<Context> context_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
