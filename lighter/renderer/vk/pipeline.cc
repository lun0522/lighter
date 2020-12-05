//
//  pipeline.cc
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline.h"

namespace lighter {
namespace renderer {
namespace vk {

Pipeline::Pipeline(SharedContext context,
                   const GraphicsPipelineDescriptor& descriptor)
    : Pipeline{std::move(context), descriptor.pipeline_name,
               VK_PIPELINE_BIND_POINT_GRAPHICS} {
  FATAL("Not implemented yet");
}

Pipeline::Pipeline(SharedContext context,
                   const ComputePipelineDescriptor& descriptor)
    : Pipeline{std::move(context), descriptor.pipeline_name,
               VK_PIPELINE_BIND_POINT_COMPUTE} {
  FATAL("Not implemented yet");
}

void Pipeline::Bind(const VkCommandBuffer& command_buffer) const {
  vkCmdBindPipeline(command_buffer, binding_point_, pipeline_);
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(*context_->device(), pipeline_,
                    *context_->host_allocator());
  vkDestroyPipelineLayout(*context_->device(), layout_,
                          *context_->host_allocator());
#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Pipeline '%s' destructed", name());
#endif  /* !NDEBUG */
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
