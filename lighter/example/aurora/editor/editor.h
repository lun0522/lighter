//
//  editor.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_EXAMPLE_AURORA_EDITOR_EDITOR_H
#define LIGHTER_EXAMPLE_AURORA_EDITOR_EDITOR_H

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "lighter/base/window.h"
#include "lighter/example/aurora/editor/celestial.h"
#include "lighter/example/aurora/editor/path.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/scene/camera.h"
#include "lighter/scene/rotation.h"
#include "lighter/scene/scene.h"
#include "lighter/ui/button.h"

namespace lighter::example::aurora {

// Interactive aurora path & viewpoint editor scene.
class Editor : public scene::Scene {
 public:
  Editor(base::Window& window, renderer::ir::Renderer& renderer,
         int window_index, renderer::ir::MultisamplingMode multisampling_mode,
         int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;

  std::vector<SplinePathView> GetSplinePathViews() {
    return aurora_path_->GetSplinePathViews();
  }

  void OnEnter() override;
  void OnExit() override;
  void OnSurfaceRecreated() override;
  void BuildFrame(renderer::ir::RenderGraph& graph, int frame_slot) override;
  bool ShouldTransitionScene() const override {
    return state_manager_.ShouldDisplayAurora();
  }

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

  // Tracks editor UI button states, active path selection, and mode
  // transitions.
  class StateManager {
   public:
    StateManager();

    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    void Update(std::optional<ButtonIndex> clicked_button);
    std::optional<int> GetSelectedPathIndex() const;

    bool IsSelected(ButtonIndex index) const {
      return button_states_[index] == ui::Button::State::kSelected;
    }
    bool IsUnselected(ButtonIndex index) const {
      return button_states_[index] == ui::Button::State::kUnselected;
    }
    bool IsEditing() const {
      return IsSelected(ButtonIndex::kEditingButtonIndex);
    }
    bool ShouldDisplayAurora() const {
      return IsSelected(ButtonIndex::kAuroraButtonIndex);
    }

    void ResetDisplayAuroraButton() {
      button_states_[ButtonIndex::kAuroraButtonIndex] =
          ui::Button::State::kUnselected;
    }

    std::span<const ui::Button::State> top_row_buttons_states() const {
      return {button_states_.data(), kNumTopRowButtons};
    }
    std::span<const ui::Button::State> bottom_row_buttons_states() const {
      return {button_states_.data() + kNumTopRowButtons, kNumBottomRowButtons};
    }

   private:
    void SetTopRowButtonsStates(ui::Button::State state);
    void SetBottomRowButtonsStates(ui::Button::State state);
    void FlipButtonState(ButtonIndex index);

    std::array<ui::Button::State, kNumButtons> button_states_{};
    std::optional<ButtonIndex> last_clicked_button_;
    ButtonIndex last_edited_path_ = kPath1ButtonIndex;
  };

  void RotateCelestials(const scene::rotation::Rotation& rotation);

  base::Window& window_;
  const int window_index_;
  const renderer::ir::MultisamplingMode multisampling_mode_;

  bool did_press_left_ = false;
  bool did_release_right_ = false;

  scene::OrthographicCameraViewedSphere earth_;
  scene::OrthographicCameraViewedSphere aurora_layer_;
  StateManager state_manager_;

  std::unique_ptr<Celestial> celestial_;
  std::unique_ptr<AuroraPath> aurora_path_;
  std::unique_ptr<ui::Button> top_row_buttons_;
  std::unique_ptr<ui::Button> bottom_row_buttons_;

  std::unique_ptr<scene::UserControlledPerspectiveCamera> skybox_camera_;
  std::unique_ptr<scene::UserControlledOrthographicCamera> general_camera_;
};

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_EDITOR_EDITOR_H
