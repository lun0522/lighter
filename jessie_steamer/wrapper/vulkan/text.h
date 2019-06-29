//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H

#include <string>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/text_util.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Text {
 public:
  using Font = CharLoader::Font;
  enum class Align { kLeft, kCenter, kRight };

  Text(SharedBasicContext context) : context_{std::move(context)} {}

 protected:
  SharedBasicContext context_;
};

class StaticText : public Text {
 public:
  StaticText(SharedBasicContext context,
             const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  StaticText(const StaticText&) = delete;
  StaticText& operator=(const StaticText&) = delete;
};

class DynamicText : public Text {
 public:
  DynamicText(SharedBasicContext context,
              const std::vector<std::string>& texts,
              Font font, int font_height)
      : Text{std::move(context)},
        char_loader_{context, texts, font, font_height} {}

  // This class is neither copyable nor movable.
  DynamicText(const DynamicText&) = delete;
  DynamicText& operator=(const DynamicText&) = delete;

  void Draw(const std::string& text,
            const glm::vec3& color, float alpha, float scale,
            float horizontal_base, float vertical_base, Align align) const;

 private:
  CharLoader char_loader_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H */
