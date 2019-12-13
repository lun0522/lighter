//
//  editor.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H

#include <array>
#include <memory>
#include <vector>

#include "jessie_steamer/application/vulkan/aurora/editor/button.h"
#include "jessie_steamer/application/vulkan/aurora/editor/celestial.h"
#include "jessie_steamer/application/vulkan/aurora/editor/path.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/rotation.h"
#include "jessie_steamer/common/spline.h"
#include "jessie_steamer/common/timer.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class Editor {
 public:
  enum ButtonIndex {
    kPath1ButtonIndex,
    kPath2ButtonIndex,
    kPath3ButtonIndex,
    kEditingButtonIndex,
    kDaylightButtonIndex,
    kAuroraButtonIndex,
    kNumButtons,
    kNumAuroraPaths = kEditingButtonIndex,
  };

  Editor(const wrapper::vulkan::WindowContext& window_context,
         int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;

  // Registers callbacks.
  void OnEnter(common::Window* mutable_window);

  // Unregisters callbacks.
  void OnExit(common::Window* mutable_window);

  void Recreate(const wrapper::vulkan::WindowContext& window_context);

  void UpdateData(const wrapper::vulkan::WindowContext& window_context,
                  int frame);

  void Render(const VkCommandBuffer& command_buffer,
              uint32_t framebuffer_index, int current_frame);

 private:
  class StateManager {
   public:
    explicit StateManager();

    // This class is neither copyable nor movable.
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    void Update(const absl::optional<ButtonIndex>& clicked_button);

    // Accessors.
    Button::State button_state(ButtonIndex index) const {
      return button_states_[index];
    }
    const std::vector<Button::State>& button_states() const {
      return button_states_;
    }

   private:
    struct ClickInfo {
      ButtonIndex button_index;
      float start_time;
    };

    void SetPathButtonStates(Button::State state);

    void FlipButtonState(ButtonIndex index);

    common::BasicTimer timer_;
    std::vector<Button::State> button_states_;
    absl::optional<ClickInfo> click_info_;
    ButtonIndex last_edited_path_ = kPath1ButtonIndex;
  };

  class PathManager {
   public:
    explicit PathManager();

    // This class is neither copyable nor movable.
    PathManager(const PathManager&) = delete;
    PathManager& operator=(const PathManager&) = delete;

    const std::vector<glm::vec3>& control_points(int path_index) const {
      return spline_editors_.at(path_index)->control_points();
    }
    const std::vector<glm::vec3>& spline_points(int path_index) const {
      return spline_editors_.at(path_index)->spline_points();
    }

   private:
    std::vector<std::unique_ptr<common::SplineEditor>> spline_editors_;
  };

  const float original_aspect_ratio_;
  bool is_pressing_left_ = false;
  common::Sphere earth_;
  StateManager state_manager_;
  PathManager path_manager_;
  std::unique_ptr<Celestial> celestial_;
  std::unique_ptr<AuroraPath> aurora_path_;
  std::unique_ptr<Button> button_;
  std::unique_ptr<common::UserControlledCamera> camera_;
  std::unique_ptr<wrapper::vulkan::NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
  std::unique_ptr<wrapper::vulkan::Image> depth_stencil_image_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H */
