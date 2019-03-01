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

 private:
  const Application& app_;
  const std::string vert_file_, frag_file_;
  VkPipelineLayout layout_;
  VkPipeline pipeline_;
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_PIPELINE_H */
