//
//  image.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image.h"

namespace lighter {
namespace renderer {
namespace vk {

DeviceImage(SharedContext context, const common::Image& image,
            bool generate_mipmaps, absl::Span<const ImageUsage> usages)
    : renderer::DeviceImage{image.dimension()},
      context_{std::move(context)} {}

DeviceImage(SharedContext context, const common::Image::Dimension& dimension,
            MultisamplingMode multisampling_mode,
            absl::Span<const ImageUsage> usages)
    : renderer::DeviceImage{dimension}, context_{std::move(context)} {}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
