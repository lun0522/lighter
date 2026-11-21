#ifndef LIGHTER_EXAMPLE_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H
#define LIGHTER_EXAMPLE_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H

#include "lighter/base/image.h"

namespace lighter::example::aurora {

// Generates a 1xN air transmittance lookup texture for atmospheric attenuation.
base::Image GenerateAirTransmitTable(float sample_step);

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H
