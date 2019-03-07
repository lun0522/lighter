//
//  pipeline.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_PIPELINE_H
#define LEARNVULKAN_PIPELINE_H

#include <string>

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {

class Application;

// fixed and programmable statges

/** VkPipeline stores the entire graphics pipeline.
 *
 *  Initialization:
 *      ShaderStage (vertex and fragment shaders)
 *      VertexInputState (how to interpret vertex attributes)
 *      InputAssemblyState (what topology to use)
 *      ViewportState (viewport and scissor)
 *      RasterizationState (lines, polygons, face culling, etc)
 *      MultisampleState (how many sample points)
 *      DepthStencilState
 *      ColorBlendState
 *      DynamicState (which properties of this pipeline will be dynamic)
 *      VkPipelineLayout (set uniform values)
 *      VkRenderPass and subpass
 *      BasePipeline (may copy settings from another pipeline)
 */
class Pipeline {
 public:
  Pipeline(const Application& app,
           const std::string& vert_file,
           const std::string& frag_file)
  : app_{app}, vert_file_{vert_file}, frag_file_{frag_file} {}
  void Init();
  void Cleanup();
  ~Pipeline() { Cleanup(); }
  MARK_NOT_COPYABLE_OR_MOVABLE(Pipeline);

  const VkPipeline& operator*(void) const { return pipeline_; }
  const VkPipelineLayout& layout()  const { return pipeline_layout_; }

 private:
  const Application& app_;
  const std::string vert_file_, frag_file_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_PIPELINE_H */
