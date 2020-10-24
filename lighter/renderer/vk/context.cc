//
//  context.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/context.h"

namespace lighter {
namespace renderer {
namespace vk {

Context::Context(
    absl::string_view application_name,
    const absl::optional<debug_message::Config>& debug_message_config)
    : application_name_{application_name},
      is_validation_enabled_{debug_message_config.has_value()} {

}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
