//
//  surface.cc
//
//  Created by Pujun Lun on 6/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/surface.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

void Surface::Init(SharedContext context) {
  context_ = std::move(context);
  surface_ = context_->window().CreateSurface(*context_->instance(),
                                              context_->allocator());
}

Surface::~Surface() {
  vkDestroySurfaceKHR(*context_->instance(), surface_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
