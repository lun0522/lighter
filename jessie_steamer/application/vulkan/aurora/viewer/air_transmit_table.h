//
//  air_transmit_table.h
//
//  Created by Pujun Lun on 4/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H

#include <memory>

#include "jessie_steamer/common/file.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

// This code is modified from Dr. Orion Sky Lawlor's implementation.
// Lawlor, Orion & Genetti, Jon. (2011). Interactive Volume Rendering Aurora on
// the GPU. Journal of WSCG. 19. 25-32.

// Generates an air transmit table texture. Such a texture enables us to look up
// how much aurora light can penetrate the air and get to our eyes in shaders.
// The size of the returned image will be [1, floor(1.0 / 'sample_step')].
// The Y coordinate, after scaled to range [0.0, 1.0], represents the cosine
// value of the angle between view ray and up vector.
std::unique_ptr<common::Image> GenerateAirTransmitTable(float sample_step);

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_AIR_TRANSMIT_TABLE_H */
