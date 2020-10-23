//
//  context.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_CONTEXT_H
#define LIGHTER_RENDERER_VK_CONTEXT_H

#include <memory>
#include <string>

#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/basic.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vk {

// Forward declarations.
class Context;

using SharedContext = std::shared_ptr<Context>;

class Context : public std::enable_shared_from_this<Context> {
 public:
  static SharedContext CreateContext(
      absl::string_view application_name,
      const absl::optional<debug_message::Config>& debug_message_config) {
    return std::shared_ptr<Context>(
        new Context{application_name, debug_message_config});
  }

  // This class is neither copyable nor movable.
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  // Accessors.
  const std::string& application_name() const { return application_name_; }
  bool is_validation_enabled() const { return is_validation_enabled_; }
  const HostMemoryAllocator& host_allocator() const { return host_allocator_; }

 private:
  explicit Context(
      absl::string_view application_name,
      const absl::optional<debug_message::Config>& debug_message_config)
      : application_name_{application_name},
        is_validation_enabled_{debug_message_config.has_value()} {}

  const std::string application_name_;

  const bool is_validation_enabled_;

  const HostMemoryAllocator host_allocator_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_CONTEXT_H */
