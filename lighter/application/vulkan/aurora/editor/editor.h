//
//  editor.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H
#define LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H

#include <array>
#include <memory>
#include <optional>

#include "lighter/application/vulkan/aurora/editor/button.h"
#include "lighter/application/vulkan/aurora/editor/celestial.h"
#include "lighter/application/vulkan/aurora/editor/path.h"
#include "lighter/application/vulkan/aurora/scene.h"
#include "lighter/application/vulkan/util.h"
#include "lighter/common/camera.h"
#include "lighter/common/rotation.h"
#include "lighter/common/window.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/window_context.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {

// This class is used to manage and render the aurora path editor scene.
// Note that to make it easier to handle user interactions with objects in
// different locations and sizes in the scene, the common::Sphere class will
// consider the center and radius of spheres, and always convert user click
// positions to object space before other computation, so that the renderer, the
// Celestial class, need not worry about it.
class Editor : public Scene {
 public:
  Editor(renderer::vulkan::WindowContext* window_context,
         int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;

  // Returns vertex buffers storing splines points that represent aurora paths.
  std::vector<const renderer::vulkan::PerVertexBuffer*>
  GetAuroraPathVertexBuffers() const {
    return aurora_path_->GetPathVertexBuffers();
  }

  // Overrides.
  void OnEnter() override;
  void OnExit() override;
  void Recreate() override;
  void UpdateData(int frame) override;
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) override;
  bool ShouldTransitionScene() const override {
    return state_manager_.ShouldDisplayAurora();
  }

  // Accessors.
  const glm::vec3& viewpoint_position() const {
    return aurora_path_->viewpoint_position();
  }

 private:
  enum ButtonIndex {
    kPath1ButtonIndex = 0,
    kPath2ButtonIndex,
    kPath3ButtonIndex,
    kViewpointButtonIndex,
    kEditingButtonIndex,
    kDaylightButtonIndex,
    kAuroraButtonIndex,
    kNumButtons,
    kNumAuroraPaths = kViewpointButtonIndex,
    kNumTopRowButtons = kEditingButtonIndex,
    kNumBottomRowButtons = kNumButtons - kNumTopRowButtons,
  };

  // Manages states of buttons.
  class StateManager {
   public:
    explicit StateManager();

    // This class is neither copyable nor movable.
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    // Updates button states. 'clicked_button' should be std::nullopt if no
    // button is clicked.
    void Update(std::optional<ButtonIndex> clicked_button);

    // Returns the index of selected aurora path. If viewpoint is selected
    // instead, returns std::nullopt.
    std::optional<int> GetSelectedPathIndex() const;

    // Convenience functions for reading button states.
    bool IsSelected(ButtonIndex index) const {
      return button_states_[index] == Button::State::kSelected;
    }
    bool IsUnselected(ButtonIndex index) const {
      return button_states_[index] == Button::State::kUnselected;
    }
    bool IsEditing() const {
      return IsSelected(ButtonIndex::kEditingButtonIndex);
    }
    bool ShouldDisplayAurora() const {
      return IsSelected(ButtonIndex::kAuroraButtonIndex);
    }

    // Resets the state of display aurora button. This should be called every
    // time we enter this scene.
    void ResetDisplayAuroraButton() {
      button_states_[ButtonIndex::kAuroraButtonIndex] =
          Button::State::kUnselected;
    }

    // Accessors.
    absl::Span<const Button::State> top_row_buttons_states() const {
      return {button_states_.data(), kNumTopRowButtons};
    }
    absl::Span<const Button::State> bottom_row_buttons_states() const {
      return {button_states_.data() + kNumTopRowButtons, kNumBottomRowButtons};
    }

   private:
    // Sets states of top row buttons to the same 'state'.
    void SetTopRowButtonsStates(Button::State state);

    // Sets states of bottom row buttons to the same 'state'.
    void SetBottomRowButtonsStates(Button::State state);

    // Flips the state of button at 'index'. This should not be called if this
    // button is hidden currently.
    void FlipButtonState(ButtonIndex index);

    // States of all buttons.
    std::array<Button::State, kNumButtons> button_states_{};

    // Records the last click on any button.
    std::optional<ButtonIndex> last_clicked_button_;

    // Tracks the index of the last edited aurora path.
    ButtonIndex last_edited_path_ = kPath1ButtonIndex;
  };

  // Rotates 'earth_' and 'aurora_layer_' together.
  void RotateCelestials(const common::rotation::Rotation& rotation);

  // Accessors.
  const renderer::vulkan::RenderPass& render_pass() const {
    return render_pass_manager_->render_pass();
  }

  // Onscreen rendering context.
  renderer::vulkan::WindowContext& window_context_;

  // Flags used for mouse button callbacks.
  bool did_press_left_ = false;
  bool did_release_right_ = false;

  // Manages render pass.
  std::unique_ptr<OnScreenRenderPassManager> render_pass_manager_;

  // Sphere models used to handle user interaction with the earth model and
  // virtual aurora layer in the scene.
  common::OrthographicCameraViewedSphere earth_;
  common::OrthographicCameraViewedSphere aurora_layer_;

  // Manages button states.
  StateManager state_manager_;

  // Renderers for objects in the scene.
  std::unique_ptr<Celestial> celestial_;
  std::unique_ptr<AuroraPath> aurora_path_;
  std::unique_ptr<Button> top_row_buttons_;
  std::unique_ptr<Button> bottom_row_buttons_;

  // Camera models. We use a perspective camera for the skybox, and an
  // orthographic camera for the earth model, so that the user need not worry
  // about the distortion of perspective camera when editing aurora paths.
  std::unique_ptr<common::UserControlledPerspectiveCamera> skybox_camera_;
  std::unique_ptr<common::UserControlledOrthographicCamera> general_camera_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H */
