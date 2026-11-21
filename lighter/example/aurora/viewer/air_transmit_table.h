//
//  air_transmit_table.h
//
//  Created by Pujun Lun on 4/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_EXAMPLE_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H
#define LIGHTER_EXAMPLE_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H

#include "lighter/common/image.h"

namespace lighter::example::aurora {

// Generates a 1xN air transmittance lookup texture for atmospheric attenuation.
common::Image GenerateAirTransmitTable(float sample_step);

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H
