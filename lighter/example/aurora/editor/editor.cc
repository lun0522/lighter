//
//  editor.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/editor/editor.h"

#include <array>
#include <format>
#include <vector>

#include "lighter/base/util.h"
#include "lighter/scene/util.h"

namespace lighter::example::aurora {
namespace {

using renderer::ir::ImageHandle;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;
using ui::Text;

constexpr float kInertialRotationDuration = 1.5f;

// Aurora layer sits at ~100km above Earth's surface.
constexpr float kEarthRadius = 6378.1f;
constexpr float kAuroraHeight = 100.0f;
constexpr float kEarthModelRadius = 1.0f;
constexpr float kAuroraLayerModelRadius =
    (kEarthRadius + kAuroraHeight) / kEarthRadius * kEarthModelRadius;

const glm::vec3& GetEarthModelCenter() {
  static const auto* earth_model_center = new glm::vec3{0.0f};
  return *earth_model_center;
}

inline glm::vec3 MakeColor(int r, int g, int b) {
  return glm::vec3{r, g, b} / 255.0f;
}

glm::vec3 GetLocation(float latitude, float longitude) {
  ASSERT_TRUE(glm::abs(latitude) <= 90.0f,
              std::format("Invalid latitude: {}", latitude));
  ASSERT_TRUE(glm::abs(longitude) <= 180.0f,
              std::format("Invalid longitude: {}", longitude));
  longitude -= 90.0f;
  const float longitude_radians = glm::radians(longitude);
  const float latitude_radians = glm::radians(latitude);
  const float cos_latitude = glm::cos(latitude_radians);
  return glm::vec3{
      cos_latitude * glm::cos(longitude_radians),
      glm::sin(latitude_radians),
      -cos_latitude * glm::sin(longitude_radians),
  };
}

std::vector<float> GetButtonCenters(int num_buttons) {
  ASSERT_TRUE(num_buttons > 0, "num_buttons must be greater than 0");
  const float button_extent = 1.0f / static_cast<float>(num_buttons);
  std::vector<float> button_centers(static_cast<size_t>(num_buttons));
  button_centers[0] = button_extent / 2.0f;
  for (int i = 1; i < num_buttons; ++i) {
    button_centers[static_cast<size_t>(i)] =
        button_centers[static_cast<size_t>(i - 1)] + button_extent;
  }
  return button_centers;
}

}  // namespace

Editor::Editor(base::Window& window, Renderer& renderer, int window_index,
               MultisamplingMode multisampling_mode, int num_frames_in_flight)
    : window_{window},
      window_index_{window_index},
      multisampling_mode_{multisampling_mode},
      earth_{GetEarthModelCenter(), kEarthModelRadius,
             kInertialRotationDuration},
      aurora_layer_{GetEarthModelCenter(), kAuroraLayerModelRadius,
                    kInertialRotationDuration} {
  const float original_aspect_ratio = window_.original_aspect_ratio();

  using ButtonColors = std::array<glm::vec3, ui::button::kNumStates>;
  const std::array<ButtonColors, kNumButtons> button_and_path_colors{
      ButtonColors{MakeColor(241, 196, 15), MakeColor(243, 156, 18)},
      ButtonColors{MakeColor(230, 126, 34), MakeColor(211, 84, 0)},
      ButtonColors{MakeColor(231, 76, 60), MakeColor(192, 57, 43)},
      ButtonColors{MakeColor(26, 188, 156), MakeColor(22, 160, 133)},
      ButtonColors{MakeColor(52, 152, 219), MakeColor(41, 128, 185)},
      ButtonColors{MakeColor(155, 89, 182), MakeColor(142, 68, 173)},
      ButtonColors{MakeColor(46, 204, 113), MakeColor(39, 174, 96)},
  };
  constexpr std::array<float, ui::button::kNumStates> kButtonAndPathAlphas{
      1.0f, 0.5f};

  celestial_ = std::make_unique<Celestial>(renderer, original_aspect_ratio,
                                           num_frames_in_flight);
  RotateCelestials({/*axis=*/{1.0f, 0.0f, 0.0f}, glm::radians(90.0f)});
  RotateCelestials({/*axis=*/{0.0f, 1.0f, 0.0f}, glm::radians(90.0f)});

  constexpr std::array<float, kNumAuroraPaths> kLatitudes{55.0f, 65.0f, 75.0f};
  constexpr int kNumControlPointsPerSpline = 8;
  constexpr int kLongitudeStep = 360 / kNumControlPointsPerSpline;
  auto generate_control_points = [&kLatitudes](int path_index) {
    const float latitude = kLatitudes.at(static_cast<size_t>(path_index));
    std::vector<glm::vec3> control_points(kNumControlPointsPerSpline);
    for (int i = 0; i < kNumControlPointsPerSpline; ++i) {
      const float longitude = static_cast<float>(kLongitudeStep * i - 180);
      control_points[static_cast<size_t>(i)] = GetLocation(latitude, longitude);
    }
    return control_points;
  };
  aurora_path_ = std::make_unique<AuroraPath>(
      renderer, num_frames_in_flight, original_aspect_ratio,
      AuroraPath::Info{
          /*max_num_control_points=*/20,
          /*control_point_radius=*/0.01f,
          /*max_recursion_depth=*/20,
          /*spline_roughness=*/1E-2f,
          /*viewpoint_initial_pos=*/GetLocation(61.2f, -149.9f),
          button_and_path_colors[kViewpointButtonIndex],
          {&button_and_path_colors[kPath1ButtonIndex], kNumAuroraPaths},
          kButtonAndPathAlphas,
          std::move(generate_control_points),
      });

  {
    using ButtonInfo = ui::Button::ButtonsInfo::Info;
    constexpr auto kFont = Text::Font::kOstrich;
    constexpr int kFontHeight = 100;
    constexpr float kBaseY = 0.25f;
    constexpr float kTopY = 0.75f;
    constexpr float kButtonHeight = 0.08f;
    const glm::vec3 text_color{1.0f};

    const std::array<std::string, kNumButtons> button_texts{
        "Path 1",  "Path 2",   "Path 3", "Viewpoint",
        "Editing", "Daylight", "Aurora",
    };

    {
      const glm::vec2 button_size{1.0f / static_cast<float>(kNumTopRowButtons),
                                  kButtonHeight};
      const auto button_centers_x = GetButtonCenters(kNumTopRowButtons);
      const float button_center_y = 1.0f - kButtonHeight / 2.0f;
      std::array<ButtonInfo, kNumTopRowButtons> button_infos{};
      for (int i = 0; i < kNumTopRowButtons; ++i) {
        button_infos[static_cast<size_t>(i)] = ButtonInfo{
            button_texts[static_cast<size_t>(i)],
            button_and_path_colors[static_cast<size_t>(i)],
            {button_centers_x[static_cast<size_t>(i)], button_center_y},
        };
      }
      top_row_buttons_ = std::make_unique<ui::Button>(
          renderer, original_aspect_ratio, num_frames_in_flight,
          ui::Button::ButtonsInfo{kFont, kFontHeight, kBaseY, kTopY, text_color,
                                  kButtonAndPathAlphas, button_size,
                                  button_infos});
    }

    {
      const glm::vec2 button_size{
          1.0f / static_cast<float>(kNumBottomRowButtons), kButtonHeight};
      const auto button_centers_x = GetButtonCenters(kNumBottomRowButtons);
      const float button_center_y = kButtonHeight / 2.0f;
      std::array<ButtonInfo, kNumBottomRowButtons> button_infos{};
      for (int i = 0; i < kNumBottomRowButtons; ++i) {
        const size_t button_index = static_cast<size_t>(kNumTopRowButtons + i);
        button_infos[static_cast<size_t>(i)] = ButtonInfo{
            button_texts[button_index],
            button_and_path_colors[button_index],
            {button_centers_x[static_cast<size_t>(i)], button_center_y},
        };
      }
      bottom_row_buttons_ = std::make_unique<ui::Button>(
          renderer, original_aspect_ratio, num_frames_in_flight,
          ui::Button::ButtonsInfo{kFont, kFontHeight, kBaseY, kTopY, text_color,
                                  kButtonAndPathAlphas, button_size,
                                  button_infos});
    }
  }

  const scene::camera_control::Config control_config;
  scene::Camera::Config camera_config;
  camera_config.position = glm::vec3{0.0f, 0.0f, 3.0f};

  skybox_camera_ = scene::UserControlledPerspectiveCamera::Create(
      control_config, camera_config,
      {/*field_of_view_y=*/45.0f, original_aspect_ratio});
  skybox_camera_->SetActivity(true);

  general_camera_ = scene::UserControlledOrthographicCamera::Create(
      control_config, camera_config,
      {/*view_width=*/3.0f, original_aspect_ratio});
  general_camera_->SetActivity(true);
}

void Editor::OnEnter() {
  did_press_left_ = false;
  did_release_right_ = false;
  window_
      .RegisterScrollCallback([this](double /*x_pos*/, double y_pos) {
        if (general_camera_->DidScroll(y_pos * 0.1f, 0.2f, 5.0f)) {
          skybox_camera_->DidScroll(y_pos, 0.0f, 90.0f);
        }
      })
      .RegisterMouseButtonCallback([this](bool is_left, bool is_press) {
        if (is_left) {
          did_press_left_ = is_press;
        } else {
          did_release_right_ = !is_press;
        }
      });
  state_manager_.ResetDisplayAuroraButton();
  OnSurfaceRecreated();
}

void Editor::OnExit() {
  window_.RegisterScrollCallback(nullptr).RegisterMouseButtonCallback(nullptr);
}

void Editor::OnSurfaceRecreated() {
  const glm::dvec2 cursor_pos = window_.GetCursorPos();
  general_camera_->SetCursorPos(cursor_pos);
  skybox_camera_->SetCursorPos(cursor_pos);
}

void Editor::BuildFrame(RenderGraph& graph, int frame_slot) {
  glm::vec2 click_ndc = window_.GetNormalizedCursorPos();
  const glm::ivec2 current_frame_size = window_.GetFrameSize();
  const float current_aspect_ratio = static_cast<float>(current_frame_size.x) /
                                     static_cast<float>(current_frame_size.y);
  const float distortion =
      current_aspect_ratio / window_.original_aspect_ratio();
  if (distortion > 1.0f) {
    click_ndc.x *= distortion;
  } else {
    click_ndc.y /= distortion;
  }

  std::optional<int> clicked_button_index;
  if (did_press_left_) {
    clicked_button_index = top_row_buttons_->GetClickedButtonIndex(
        click_ndc, /*button_index_offset=*/0,
        state_manager_.top_row_buttons_states());
    if (!clicked_button_index.has_value()) {
      clicked_button_index = bottom_row_buttons_->GetClickedButtonIndex(
          click_ndc, /*button_index_offset=*/kNumTopRowButtons,
          state_manager_.bottom_row_buttons_states());
    }
  }
  std::optional<ButtonIndex> clicked_button;
  if (clicked_button_index.has_value()) {
    clicked_button = static_cast<ButtonIndex>(clicked_button_index.value());
  }
  state_manager_.Update(clicked_button);

  const scene::OrthographicCamera& general_camera = general_camera_->camera();
  std::optional<glm::vec2> click_earth_ndc;
  std::optional<AuroraPath::ClickInfo> click_celestial;
  if (!clicked_button.has_value()) {
    if (state_manager_.IsEditing()) {
      if (did_press_left_ || did_release_right_) {
        const auto selected_path_index = state_manager_.GetSelectedPathIndex();
        const auto& celestial_to_intersect =
            selected_path_index.has_value() ? aurora_layer_ : earth_;
        const auto intersection =
            celestial_to_intersect.GetIntersection(general_camera, click_ndc);
        if (intersection.has_value()) {
          click_celestial = AuroraPath::ClickInfo{
              selected_path_index,
              /*is_left_click=*/!did_release_right_,
              intersection.value(),
          };
        }
      }
    } else if (did_press_left_) {
      click_earth_ndc = click_ndc;
    }
  }

  const auto rotation = earth_.ShouldRotate(general_camera, click_earth_ndc);
  if (rotation.has_value()) {
    RotateCelestials(rotation.value());
  }
  did_release_right_ = false;

  const auto earth_texture_index =
      state_manager_.IsSelected(kDaylightButtonIndex)
          ? Celestial::kEarthDayTextureIndex
          : Celestial::kEarthNightTextureIndex;
  const glm::mat4 earth_transform_matrix =
      general_camera.GetProjectionMatrix() * general_camera.GetViewMatrix() *
      earth_.model_matrix();
  const auto& skybox_camera = skybox_camera_->camera();
  const glm::mat4 skybox_transform_matrix =
      skybox_camera.GetProjectionMatrix() *
      skybox_camera.GetSkyboxViewMatrix() *
      earth_.GetSkyboxModelMatrix(/*scale=*/1.5f);
  const std::optional<int> selected_path =
      state_manager_.GetSelectedPathIndex();

  const ImageHandle backbuffer = graph.ImportSwapchainImage(window_index_);
  graph.AddGraphicsPass(
      "AuroraEditor",
      [&](PassBuilder& builder) {
        scene::SetupMainPassAttachments(builder, backbuffer,
                                        multisampling_mode_,
                                        /*use_depth=*/true);
      },
      [this, backbuffer, frame_slot, earth_texture_index,
       earth_transform_matrix, skybox_transform_matrix, selected_path,
       click_celestial](PassContext& ctx) {
        const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
        celestial_->Draw(ctx, frame_size, frame_slot, earth_texture_index,
                         earth_transform_matrix, skybox_transform_matrix);
        aurora_path_->UpdateAndDraw(
            ctx, frame_size, frame_slot, general_camera_->camera(),
            aurora_layer_.model_matrix(), kAuroraLayerModelRadius,
            selected_path, click_celestial);
        top_row_buttons_->Draw(ctx, frame_size, frame_slot,
                               state_manager_.top_row_buttons_states());
        bottom_row_buttons_->Draw(ctx, frame_size, frame_slot,
                                  state_manager_.bottom_row_buttons_states());
      });
}

void Editor::RotateCelestials(const scene::rotation::Rotation& rotation) {
  earth_.Rotate(rotation);
  aurora_layer_.Rotate(rotation);
}

Editor::StateManager::StateManager() {
  SetTopRowButtonsStates(ui::Button::State::kHidden);
  SetBottomRowButtonsStates(ui::Button::State::kUnselected);
}

void Editor::StateManager::Update(std::optional<ButtonIndex> clicked_button) {
  if (!clicked_button.has_value() || clicked_button == last_clicked_button_) {
    last_clicked_button_ = clicked_button;
    return;
  }

  const ButtonIndex button_index = clicked_button.value();
  if (button_index < kNumTopRowButtons) {
    if (IsUnselected(button_index)) {
      FlipButtonState(last_edited_path_);
      FlipButtonState(button_index);
      last_edited_path_ = button_index;
    }
  } else {
    FlipButtonState(button_index);
    if (button_index == kEditingButtonIndex) {
      if (IsEditing()) {
        SetTopRowButtonsStates(ui::Button::State::kUnselected);
        FlipButtonState(last_edited_path_);
      } else {
        SetTopRowButtonsStates(ui::Button::State::kHidden);
      }
    }
  }
  last_clicked_button_ = clicked_button;
}

std::optional<int> Editor::StateManager::GetSelectedPathIndex() const {
  for (int i = kPath1ButtonIndex; i < ButtonIndex::kNumAuroraPaths; ++i) {
    const auto button_index =
        static_cast<ButtonIndex>(ButtonIndex::kPath1ButtonIndex + i);
    if (IsSelected(button_index)) {
      return i;
    }
  }
  return std::nullopt;
}

void Editor::StateManager::SetTopRowButtonsStates(ui::Button::State state) {
  for (int i = 0; i < kNumTopRowButtons; ++i) {
    button_states_[static_cast<size_t>(i)] = state;
  }
}

void Editor::StateManager::SetBottomRowButtonsStates(ui::Button::State state) {
  for (int i = kNumTopRowButtons; i < kNumButtons; ++i) {
    button_states_[static_cast<size_t>(i)] = state;
  }
}

void Editor::StateManager::FlipButtonState(ButtonIndex index) {
  switch (button_states_[index]) {
    case ui::Button::State::kHidden:
      FATAL("Should not call on a hidden button");
    case ui::Button::State::kSelected:
      button_states_[index] = ui::Button::State::kUnselected;
      break;
    case ui::Button::State::kUnselected:
      button_states_[index] = ui::Button::State::kSelected;
      break;
  }
}

}  // namespace lighter::example::aurora
