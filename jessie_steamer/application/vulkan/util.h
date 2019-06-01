//
//  util.h
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H

#include <cstdlib>

namespace jessie_steamer {
namespace application {
namespace vulkan {

inline void SetBuildEnvironment() {
  setenv("VK_ICD_FILENAMES",
         "external/lib-vulkan/etc/vulkan/icd.d/MoltenVK_icd.json", 1);
#ifndef NDEBUG
  setenv("VK_LAYER_PATH", "external/lib-vulkan/etc/vulkan/explicit_layer.d", 1);
#endif /* !NDEBUG */
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H */
