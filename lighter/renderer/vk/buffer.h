//
//  buffer.h
//
//  Created by Pujun Lun on 10/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_H
#define LIGHTER_RENDERER_VK_BUFFER_H

#include "lighter/renderer/buffer.h"
#include "lighter/renderer/vk/context.h"

namespace lighter {
namespace renderer {
namespace vk {

class DeviceBuffer : public renderer::DeviceBuffer {
 public:
  // This class is neither copyable nor movable.
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;

 private:

};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BUFFER_H */
