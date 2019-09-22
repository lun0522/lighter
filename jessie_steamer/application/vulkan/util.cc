//
//  util.cc
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/util.h"

ABSL_FLAG(bool, performance_mode, false,
          "Ignore VSync and present images to the screen as fast as possible");
