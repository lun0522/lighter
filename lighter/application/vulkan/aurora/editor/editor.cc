//
//  editor.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/editor/editor.h"

#include <vector>

#include "lighter/application/vulkan/aurora/editor/button_util.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace renderer::vulkan;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kAuroraPathSubpassIndex,
  kButtonSubpassIndex,
  kNumSubpasses,
};

constexpr float kInertialRotationDuration = 1.5f;

// The height of aurora layer is assumed to be at around 100km above the ground.
constexpr float kEarthRadius = 6378.1f;
constexpr float kAuroraHeight = 100.0f;
constexpr float kEarthModelRadius = 1.0f;
constexpr float kAuroraLayerModelRadius =
    (kEarthRadius + kAuroraHeight) / kEarthRadius * kEarthModelRadius;

// Returns the coordinate of earth model center.
const glm::vec3& GetEarthModelCenter() {
  static const auto* earth_model_center = new glm::vec3{0.0f};
  return *earth_model_center;
}

// Converts RGB color from range [0, 255] to [0.0, 1.0].
inline glm::vec3 MakeColor(int r, int g, int b) {
  return glm::vec3{r, g, b} / 255.0f;
}

// Returns a point that has the given 'latitude' and 'longitude', which are
// measured in degrees. North latitude and East longitude are positive, while
// South latitude and West longitude are negative.
glm::vec3 GetLocation(float latitude, float longitude) {
  ASSERT_TRUE(glm::abs(latitude) <= 90.0f,
              absl::StrFormat("Invalid latitude: %f", latitude));
  ASSERT_TRUE(glm::abs(longitude) <= 180.0f,
              absl::StrFormat("Invalid longitude: %f", longitude));
  // Offset is determined by the location of prime meridian on earth textures.
  longitude -= 90.0f;
  const float longitude_radians = glm::radians(longitude);
  const float latitude_radians = glm::radians(latitude);
  const float cos_latitude = glm::cos(latitude_radians);
  return glm::vec3{
      /*x=*/cos_latitude * glm::cos(longitude_radians),
      /*y=*/glm::sin(latitude_radians),
      /*z=*/-cos_latitude * glm::sin(longitude_radians),
  };
}

// Distributes buttons evenly within range [0.0, 1.0].
std::vector<float> GetButtonCenters(int num_buttons) {
  ASSERT_TRUE(num_buttons > 0, "num_buttons must be greater than 0");
  const float button_extent = 1.0f / static_cast<float>(num_buttons);
  std::vector<float> button_centers(num_buttons);
  button_centers[0] = button_extent / 2.0f;
  for (int i = 1; i < num_buttons; ++i) {
    button_centers[i] = button_centers[i - 1] + button_extent;
  }
  return button_centers;
}

} /* namespace */

Editor::Editor(WindowContext* window_context, int num_frames_in_flight)
    : window_context_{*FATAL_IF_NULL(window_context)},
      earth_{GetEarthModelCenter(), kEarthModelRadius,
             kInertialRotationDuration},
      aurora_layer_{GetEarthModelCenter(), kAuroraLayerModelRadius,
                    kInertialRotationDuration} {
  // Prevent shaders from being auto released.
  ShaderModule::AutoReleaseShaderPool shader_pool;

  const auto context = window_context_.basic_context();
  const float original_aspect_ratio = window_context_.original_aspect_ratio();

  // Buttons and paths share color and alpha values.
  using ButtonColors = std::array<glm::vec3, button::kNumStates>;
  const std::array<ButtonColors, kNumButtons> button_and_path_colors{
      ButtonColors{MakeColor(241, 196,  15), MakeColor(243, 156,  18)},
      ButtonColors{MakeColor(230, 126,  34), MakeColor(211,  84,   0)},
      ButtonColors{MakeColor(231,  76,  60), MakeColor(192,  57,  43)},
      ButtonColors{MakeColor( 26, 188, 156), MakeColor( 22, 160, 133)},
      ButtonColors{MakeColor( 52, 152, 219), MakeColor( 41, 128, 185)},
      ButtonColors{MakeColor(155,  89, 182), MakeColor(142,  68, 173)},
      ButtonColors{MakeColor( 46, 204, 113), MakeColor( 39, 174,  96)},
  };
  constexpr std::array<float, button::kNumStates>
      kButtonAndPathAlphas{1.0f, 0.5f};

  /* Render pass */
  render_pass_manager_ = std::make_unique<OnScreenRenderPassManager>(
      &window_context_,
      NaiveRenderPass::SubpassConfig{
          kNumSubpasses, /*first_transparent_subpass=*/kAuroraPathSubpassIndex,
          /*first_overlay_subpass=*/kButtonSubpassIndex});

  /* Earth and skybox */
  celestial_ = std::make_unique<Celestial>(
      context, original_aspect_ratio, num_frames_in_flight);

  // Initially, the north pole points to the center of frame.
  RotateCelestials({/*axis=*/{1.0f, 0.0f, 0.0f},
                    /*angle=*/glm::radians(90.0f)});
  RotateCelestials({/*axis=*/{0.0f, 1.0f, 0.0f},
                    /*angle=*/glm::radians(90.0f)});

  /* Aurora path */
  constexpr std::array<float, kNumAuroraPaths> kLatitudes{55.0f, 65.0f, 75.0f};
  constexpr int kNumControlPointsPerSpline = 8;
  constexpr int kLongitudeStep = 360 / kNumControlPointsPerSpline;
  auto generate_control_points = [&kLatitudes](int path_index) {
    const float latitude = kLatitudes.at(path_index);
    std::vector<glm::vec3> control_points(kNumControlPointsPerSpline);
    for (int i = 0; i < control_points.size(); ++i) {
      const auto longitude = static_cast<float>(kLongitudeStep * i - 180);
      control_points[i] = GetLocation(latitude, longitude);
    }
    return control_points;
  };
  // Initially, the viewpoint is located at Anchorage, AK, USA.
  aurora_path_ = std::make_unique<AuroraPath>(
      context, num_frames_in_flight, original_aspect_ratio, AuroraPath::Info{
          /*max_num_control_points=*/20, /*control_point_radius=*/0.01f,
          /*max_recursion_depth=*/20, /*spline_smoothness=*/1E-2,
          /*viewpoint_initial_pos=*/GetLocation(61.2f, -149.9f),
          button_and_path_colors[kViewpointButtonIndex],
          {&button_and_path_colors[kPath1ButtonIndex], kNumAuroraPaths},
          kButtonAndPathAlphas, std::move(generate_control_points),
      });

  /* Buttons */
  {
    using ButtonInfo = Button::ButtonsInfo::Info;
    constexpr auto kFont = Text::Font::kOstrich;
    constexpr int kFontHeight = 100;
    constexpr float kBaseY = 0.25f;
    constexpr float kTopY = 0.75f;
    constexpr float kButtonHeight = 0.08f;
    const glm::vec3 text_color{1.0f};

    const std::array<std::string, kNumButtons> button_texts{
        "Path 1", "Path 2", "Path 3", "Viewpoint",
        "Editing", "Daylight", "Aurora",
    };

    /* Top row buttons */
    {
      const glm::vec2 button_size{1.0f / kNumTopRowButtons, kButtonHeight};
      const auto button_centers_x = GetButtonCenters(kNumTopRowButtons);
      const float button_center_y = 1.0f - kButtonHeight / 2.0f;
      const auto get_button_info = [&](int button_index) {
        return ButtonInfo{
            button_texts[button_index],
            button_and_path_colors[button_index],
            {button_centers_x[button_index], button_center_y},
        };
      };
      std::array<ButtonInfo, kNumTopRowButtons> button_infos{};
      for (int i = 0; i < kNumTopRowButtons; ++i) {
        button_infos[i] = get_button_info(i);
      }
      top_row_buttons_ = std::make_unique<Button>(
          context, original_aspect_ratio, Button::ButtonsInfo{
              kFont, kFontHeight, kBaseY, kTopY, text_color,
              kButtonAndPathAlphas, button_size, button_infos});
    }

    /* Bottom row buttons */
    {
      const glm::vec2 button_size{1.0f / kNumBottomRowButtons, kButtonHeight};
      const auto button_centers_x = GetButtonCenters(kNumBottomRowButtons);
      const float button_center_y = kButtonHeight / 2.0f;
      const auto get_button_info = [&](int relative_index) {
        const int button_index = kNumTopRowButtons + relative_index;
        return ButtonInfo{
            button_texts[button_index],
            button_and_path_colors[button_index],
            {button_centers_x[relative_index], button_center_y},
        };
      };
      std::array<ButtonInfo, kNumBottomRowButtons> button_infos{};
      for (int i = 0; i < kNumBottomRowButtons; ++i) {
        button_infos[i] = get_button_info(i);
      }
      bottom_row_buttons_ = std::make_unique<Button>(
          context, original_aspect_ratio, Button::ButtonsInfo{
              kFont, kFontHeight, kBaseY, kTopY, text_color,
              kButtonAndPathAlphas, button_size, button_infos});
    }
  }

  /* Camera */
  const common::camera_control::Config control_config;
  common::Camera::Config camera_config;
  camera_config.position = glm::vec3{0.0f, 0.0f, 3.0f};

  const common::PerspectiveCamera::FrustumConfig frustum_config{
      /*field_of_view_y=*/45.0f, original_aspect_ratio};
  skybox_camera_ = common::UserControlledPerspectiveCamera::Create(
      control_config, camera_config, frustum_config);
  skybox_camera_->SetActivity(true);

  const common::OrthographicCamera::OrthoConfig ortho_config{
      /*view_width=*/3.0f, original_aspect_ratio};
  general_camera_ = common::UserControlledOrthographicCamera::Create(
      control_config, camera_config, ortho_config);
  general_camera_->SetActivity(true);
}

void Editor::OnEnter() {
  did_press_left_ = false;
  did_release_right_ = false;
  (*window_context_.mutable_window())
      .RegisterScrollCallback([this](double x_pos, double y_pos) {
        // Since we have two cameras, to make sure they always zoom in/out
        // together, we don't set real limits to the skybox camera, and let the
        // general camera determine whether or not to zoom in/out.
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
}

void Editor::OnExit() {
  (*window_context_.mutable_window())
      .RegisterScrollCallback(nullptr)
      .RegisterMouseButtonCallback(nullptr);
}

void Editor::Recreate() {
  // Prevent shaders from being auto released.
  ShaderModule::AutoReleaseShaderPool shader_pool;

  render_pass_manager_->RecreateRenderPass();

  general_camera_->SetCursorPos(window_context_.window().GetCursorPos());
  skybox_camera_->SetCursorPos(window_context_.window().GetCursorPos());

  const VkExtent2D& frame_size = window_context_.frame_size();
  const VkSampleCountFlagBits sample_count = window_context_.sample_count();
  celestial_->UpdateFramebuffer(frame_size, sample_count, render_pass(),
                                kModelSubpassIndex);
  aurora_path_->UpdateFramebuffer(frame_size, sample_count, render_pass(),
                                  kAuroraPathSubpassIndex);
  top_row_buttons_->UpdateFramebuffer(frame_size, sample_count, render_pass(),
                                      kButtonSubpassIndex);
  bottom_row_buttons_->UpdateFramebuffer(frame_size, sample_count,
                                         render_pass(), kButtonSubpassIndex);
}

void Editor::UpdateData(int frame) {
  glm::vec2 click_ndc = window_context_.window().GetNormalizedCursorPos();
  // When the frame is resized, the viewport is changed to maintain the aspect
  // ratio, hence we need to consider the distortion caused by viewport changes.
  const float current_aspect_ratio =
      util::GetAspectRatio(window_context_.frame_size());
  const float distortion =
      current_aspect_ratio / window_context_.original_aspect_ratio();
  if (distortion > 1.0f) {
    click_ndc.x *= distortion;
  } else {
    click_ndc.y /= distortion;
  }

  // Process clicking on button.
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

  // Process interaction with earth or aurora layer if no button is clicked.
  const common::OrthographicCamera& general_camera = general_camera_->camera();
  std::optional<glm::vec2> click_earth_ndc;
  std::optional<AuroraPath::ClickInfo> click_celestial;
  if (!clicked_button.has_value()) {
    if (state_manager_.IsEditing()) {
      // If editing aurora paths, intersect with aurora layer. If editing
      // viewpoint, intersect with earth.
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
      // If not in editing mode, only interact with earth.
      click_earth_ndc = click_ndc;
    }
  }

  // Compute earth rotation.
  const auto rotation = earth_.ShouldRotate(general_camera, click_earth_ndc);
  if (rotation.has_value()) {
    RotateCelestials(rotation.value());
  }

  // Update earth, aurora and skybox.
  const auto earth_texture_index =
      state_manager_.IsSelected(kDaylightButtonIndex)
          ? Celestial::kEarthDayTextureIndex
          : Celestial::kEarthNightTextureIndex;
  const glm::mat4 earth_transform_matrix =
      general_camera.GetProjectionMatrix() * general_camera.GetViewMatrix() *
      earth_.model_matrix();
  celestial_->UpdateEarthData(frame, earth_texture_index,
                              earth_transform_matrix);

  const auto& skybox_camera = skybox_camera_->camera();
  const glm::mat4 skybox_transform_matrix =
      skybox_camera.GetProjectionMatrix() *
      skybox_camera.GetSkyboxViewMatrix() *
      earth_.GetSkyboxModelMatrix(/*scale=*/1.5f);
  celestial_->UpdateSkyboxData(frame, skybox_transform_matrix);

  aurora_path_->UpdatePerFrameData(
      frame, general_camera, aurora_layer_.model_matrix(),
      kAuroraLayerModelRadius, click_celestial);

  // Reset right mouse button flag.
  did_release_right_ = false;
}

void Editor::Draw(const VkCommandBuffer& command_buffer,
                  uint32_t framebuffer_index, int current_frame) {
  render_pass().Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        celestial_->Draw(command_buffer, current_frame);
      },
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        aurora_path_->Draw(command_buffer, current_frame,
                           state_manager_.GetSelectedPathIndex());
      },
      [this](const VkCommandBuffer& command_buffer) {
        top_row_buttons_->Draw(
            command_buffer, state_manager_.top_row_buttons_states());
        bottom_row_buttons_->Draw(
            command_buffer, state_manager_.bottom_row_buttons_states());
      },
  });
}

void Editor::RotateCelestials(const common::rotation::Rotation& rotation) {
  earth_.Rotate(rotation);
  aurora_layer_.Rotate(rotation);
}

Editor::StateManager::StateManager() {
  SetTopRowButtonsStates(Button::State::kHidden);
  SetBottomRowButtonsStates(Button::State::kUnselected);
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
        SetTopRowButtonsStates(Button::State::kUnselected);
        FlipButtonState(last_edited_path_);
      } else {
        SetTopRowButtonsStates(Button::State::kHidden);
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

void Editor::StateManager::SetTopRowButtonsStates(Button::State state) {
  for (int i = 0; i < kNumTopRowButtons; ++i) {
    button_states_[i] = state;
  }
}

void Editor::StateManager::SetBottomRowButtonsStates(Button::State state) {
  for (int i = kNumTopRowButtons; i < kNumButtons; ++i) {
    button_states_[i] = state;
  }
}

void Editor::StateManager::FlipButtonState(ButtonIndex index) {
  switch (button_states_[index]) {
    case Button::State::kHidden:
      FATAL("Should not call on a hidden button");
    case Button::State::kSelected:
      button_states_[index] = Button::State::kUnselected;
      break;
    case Button::State::kUnselected:
      button_states_[index] = Button::State::kSelected;
      break;
  }
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
