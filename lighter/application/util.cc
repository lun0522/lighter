//
//  util.cc
//
//  Created by Pujun Lun on 8/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/util.h"

ABSL_FLAG(bool, performance_mode, false,
          "Ignore VSync and present images to the screen as fast as possible");
