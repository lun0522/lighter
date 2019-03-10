//
//  pipeline.h
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_PIPELINE_H
#define WRAPPER_VULKAN_PIPELINE_H

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "buffer.h"

namespace wrapper {
namespace vulkan {

class Context;

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
  Pipeline() = default;
  void Init(std::shared_ptr<Context> context,
            const std::string& vert_file,
            const std::string& frag_file,
            const UniformBuffer& uniform_buffer,
            const std::vector<VkVertexInputBindingDescription>& binding_descs,
            const std::vector<VkVertexInputAttributeDescription>& attrib_descs);
  void Cleanup();
  ~Pipeline() { Cleanup(); }

  // This class is neither copyable nor movable
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  const VkPipeline& operator*(void) const { return pipeline_; }
  const VkPipelineLayout& layout()  const { return pipeline_layout_; }

 private:
  std::shared_ptr<Context> context_;
  std::string vert_file_, frag_file_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_PIPELINE_H */
