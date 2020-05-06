//
//  lighting_pass.h
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace troop {

class LightingPass {
 public:
  // This class is neither copyable nor movable.
  LightingPass(const LightingPass&) = delete;
  LightingPass& operator=(const LightingPass&) = delete;

 private:
};

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H */
