//
//  editor.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/editor.h"

#include <array>

#include "jessie_steamer/application/vulkan/aurora/editor/state.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

using std::array;
using std::vector;

using ButtonColors = array<glm::vec3, state::kNumStates>;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kAuroraPathSubpassIndex,
  kButtonSubpassIndex,
  kNumSubpasses,
  kNumTransparentSubpasses = kButtonSubpassIndex - kAuroraPathSubpassIndex,
  kNumOverlaySubpasses = kNumSubpasses - kButtonSubpassIndex,
};

constexpr float kButtonBounceTime = 0.5f;
constexpr float kInertialRotationDuration = 1.5f;

// The height of aurora layer is assumed to be at around 100km above the ground.
constexpr float kEarthRadius = 6378.1f;
constexpr float kAuroraHeight = 100.0f;
constexpr float kEarthModelRadius = 1.0f;
constexpr float kAuroraLayerModelRadius =
    (kEarthRadius + kAuroraHeight) / kEarthRadius * kEarthModelRadius;

const glm::vec3& GetEarthModelCenter() {
  static const glm::vec3* earth_model_center = nullptr;
  if (earth_model_center == nullptr) {
    earth_model_center = new glm::vec3{0.0f};
  }
  return *earth_model_center;
}

inline glm::vec3 MakeColor(int r, int g, int b) {
  return glm::vec3{r, g, b} / 255.0f;
}

const array<ButtonColors, Editor::kNumButtons>& GetAllButtonColors() {
  static array<ButtonColors, Editor::kNumButtons>* all_button_colors = nullptr;
  if (all_button_colors == nullptr) {
    all_button_colors = new array<ButtonColors, Editor::kNumButtons>{
        ButtonColors{MakeColor(241, 196,  15), MakeColor(243, 156,  18)},
        ButtonColors{MakeColor(230, 126,  34), MakeColor(211,  84,   0)},
        ButtonColors{MakeColor(231,  76,  60), MakeColor(192,  57,  43)},
        ButtonColors{MakeColor( 52, 152, 219), MakeColor( 41, 128, 185)},
        ButtonColors{MakeColor(155,  89, 182), MakeColor(142,  68, 173)},
        ButtonColors{MakeColor( 46, 204, 113), MakeColor( 39, 174,  96)},
    };
  }
  return *all_button_colors;
}

const array<float, state::kNumStates>& GetButtonAlphas() {
  static array<float, state::kNumStates>* button_alphas = nullptr;
  if (button_alphas == nullptr) {
    button_alphas = new array<float, state::kNumStates>{1.0f, 0.5f};
  }
  return *button_alphas;
}

} /* namespace */

Editor::Editor(const WindowContext& window_context,
               int num_frames_in_flight)
    : earth_{GetEarthModelCenter(), kEarthModelRadius,
             kInertialRotationDuration},
      aurora_layer_{GetEarthModelCenter(), kAuroraLayerModelRadius,
                    kInertialRotationDuration} {
  const auto context = window_context.basic_context();
  const float original_aspect_ratio =
      window_context.window().original_aspect_ratio();

  /* Earth and skybox */
  celestial_ = absl::make_unique<Celestial>(
      context, original_aspect_ratio, num_frames_in_flight);

  /* Aurora path */
  constexpr array<float, kNumAuroraPaths> kLatitudes{55.0f, 65.0f, 75.0f};
  constexpr int kNumControlPointsPerSpline = 8;
  constexpr int kLongitudeStep = 360 / kNumControlPointsPerSpline;
  auto generate_control_points =
      [&kLatitudes](int path_index) -> vector<glm::vec3> {
        const float latitude = kLatitudes.at(path_index);
        const float latitude_radians = glm::radians(latitude);
        const float sin_latitude = glm::sin(latitude_radians);
        const float cos_latitude = glm::cos(latitude_radians);

        vector<glm::vec3> control_points(kNumControlPointsPerSpline);
        for (int i = 0; i < control_points.size(); ++i) {
          const float longitude_radians =
              glm::radians(static_cast<float>(kLongitudeStep * i));
          control_points[i] = glm::vec3{
              glm::cos(longitude_radians) * cos_latitude,
              sin_latitude,
              glm::sin(longitude_radians) * cos_latitude,
          };
        }
        return control_points;
      };

  aurora_path_ = absl::make_unique<AuroraPath>(
      context, num_frames_in_flight, original_aspect_ratio, AuroraPath::Info{
          /*max_num_control_points=*/20, /*control_point_radius=*/0.015f,
          /*max_recursion_depth=*/20, /*spline_smoothness=*/1E-2,
          /*path_colors=*/vector<array<glm::vec3, state::kNumStates>>{
              GetAllButtonColors()[kPath1ButtonIndex],
              GetAllButtonColors()[kPath2ButtonIndex],
              GetAllButtonColors()[kPath3ButtonIndex],
          },
          /*path_alphas=*/GetButtonAlphas(), std::move(generate_control_points),
      });

  /* Button */
  using button::ButtonsInfo;
  const ButtonsInfo buttons_info{
      Text::Font::kOstrich, /*font_height=*/100, /*base_y=*/0.25f,
      /*top_y=*/0.75f, /*text_color=*/glm::vec3{1.0f}, GetButtonAlphas(),
      /*button_size=*/glm::vec2{0.2f, 0.1f}, /*button_infos=*/{
          ButtonsInfo::Info{
              "Path 1", GetAllButtonColors()[kPath1ButtonIndex],
              /*center=*/glm::vec2{0.2f, 0.9f},
          },
          ButtonsInfo::Info{
              "Path 2", GetAllButtonColors()[kPath2ButtonIndex],
              /*center=*/glm::vec2{0.5f, 0.9f},
          },
          ButtonsInfo::Info{
              "Path 3", GetAllButtonColors()[kPath3ButtonIndex],
              /*center=*/glm::vec2{0.8f, 0.9f},
          },
          ButtonsInfo::Info{
              "Editing", GetAllButtonColors()[kEditingButtonIndex],
              /*center=*/glm::vec2{0.2f, 0.1f},
          },
          ButtonsInfo::Info{
              "Daylight", GetAllButtonColors()[kDaylightButtonIndex],
              /*center=*/glm::vec2{0.5f, 0.1f},
          },
          ButtonsInfo::Info{
              "Aurora", GetAllButtonColors()[kAuroraButtonIndex],
              /*center=*/glm::vec2{0.8f, 0.1f},
          },
      },
  };
  button_ = absl::make_unique<Button>(
      context, original_aspect_ratio, buttons_info);

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{0.0f, 0.0f, 3.0f};
  const common::UserControlledCamera::ControlConfig camera_control_config{};

  const common::PerspectiveCamera::PersConfig pers_config{
      original_aspect_ratio};
  skybox_camera_ = absl::make_unique<common::UserControlledCamera>(
      camera_control_config,
      absl::make_unique<common::PerspectiveCamera>(config, pers_config));
  skybox_camera_->SetActivity(true);

  const common::OrthographicCamera::OrthoConfig ortho_config{
      /*view_width=*/3.0f, original_aspect_ratio};
  general_camera_ = absl::make_unique<common::UserControlledCamera>(
      camera_control_config,
      absl::make_unique<common::OrthographicCamera>(config, ortho_config));
  general_camera_->SetActivity(true);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/true,
      kNumTransparentSubpasses,
      kNumOverlaySubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context, subpass_config,
      /*num_framebuffers=*/window_context.num_swapchain_images(),
      /*present_to_screen=*/true, window_context.multisampling_mode());
}

void Editor::OnEnter(common::Window* mutable_window) {
  (*mutable_window)
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
}

void Editor::OnExit(common::Window* mutable_window) {
  (*mutable_window)
      .RegisterScrollCallback(nullptr)
      .RegisterMouseButtonCallback(nullptr);
}

void Editor::Recreate(const WindowContext& window_context) {
  /* Camera */
  general_camera_->SetCursorPos(window_context.window().GetCursorPos());
  skybox_camera_->SetCursorPos(window_context.window().GetCursorPos());

  /* Depth image */
  const VkExtent2D& frame_size = window_context.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      window_context.basic_context(), frame_size,
      window_context.multisampling_mode());

  /* Render pass */
  (*render_pass_builder_->mutable_builder())
      .UpdateAttachmentImage(
          render_pass_builder_->color_attachment_index(),
          [&window_context](int framebuffer_index) -> const Image& {
            return window_context.swapchain_image(framebuffer_index);
          })
      .UpdateAttachmentImage(
          render_pass_builder_->depth_attachment_index(),
          [this](int framebuffer_index) -> const Image& {
            return *depth_stencil_image_;
          });
  if (render_pass_builder_->has_multisample_attachment()) {
    render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
        render_pass_builder_->multisample_attachment_index(),
        [&window_context](int framebuffer_index) -> const Image& {
          return window_context.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)->Build();

  /* Objects in scene */
  const VkSampleCountFlagBits sample_count = window_context.sample_count();
  celestial_->UpdateFramebuffer(frame_size, sample_count, *render_pass_,
                                kModelSubpassIndex);
  aurora_path_->UpdateFramebuffer(frame_size, sample_count, *render_pass_,
                                  kAuroraPathSubpassIndex);
  button_->UpdateFramebuffer(frame_size, sample_count, *render_pass_,
                             kButtonSubpassIndex);
}

void Editor::UpdateData(const WindowContext& window_context, int frame) {
  glm::vec2 click_ndc = window_context.window().GetNormalizedCursorPos();
  // When the frame is resized, the viewport is changed to maintain the aspect
  // ratio, hence we need to consider the distortion caused by viewport changes.
  const float current_aspect_ratio =
      util::GetAspectRatio(window_context.frame_size());
  const float distortion =
      current_aspect_ratio / window_context.window().original_aspect_ratio();
  if (distortion > 1.0f) {
    click_ndc.x *= distortion;
  } else {
    click_ndc.y /= distortion;
  }

  // Process clicking on button.
  absl::optional<int> clicked_button_index;
  if (did_press_left_) {
    clicked_button_index = button_->GetClickedButtonIndex(
        click_ndc, state_manager_.button_states());
  }
  absl::optional<ButtonIndex> clicked_button;
  if (clicked_button_index.has_value()) {
    clicked_button = static_cast<ButtonIndex>(clicked_button_index.value());
  }
  state_manager_.Update(clicked_button);

  // Process interaction with earth or aurora layer if no button is clicked.
  const auto& general_camera = dynamic_cast<const common::OrthographicCamera&>(
      general_camera_->camera());
  absl::optional<glm::vec2> click_earth_ndc;
  absl::optional<AuroraPath::ClickInfo> click_aurora_layer;
  if (!clicked_button.has_value()) {
    if (state_manager_.IsEditing()) {
      // If in editing mode, only interact with aurora layer.
      if (did_press_left_ || did_release_right_) {
        const auto intersection =
            aurora_layer_.GetIntersection(general_camera, click_ndc);
        if (intersection.has_value()) {
          click_aurora_layer = AuroraPath::ClickInfo{
              state_manager_.GetEditingPathIndex(),
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
    earth_.Rotate(rotation.value());
    aurora_layer_.Rotate(rotation.value());
  }

  // Update earth, aurora and skybox.
  const auto earth_texture_index =
      state_manager_.IsSelected(kDaylightButtonIndex)
          ? Celestial::kEarthDayTextureIndex
          : Celestial::kEarthNightTextureIndex;
  const glm::mat4 earth_transform_matrix = general_camera.projection() *
                                           general_camera.view() *
                                           earth_.model_matrix();
  celestial_->UpdateEarthData(frame, earth_texture_index,
                              earth_transform_matrix);

  const auto& skybox_camera = skybox_camera_->camera();
  const glm::mat4 skybox_transform_matrix =
      skybox_camera.projection() *
      skybox_camera.GetSkyboxViewMatrix() *
      earth_.GetSkyboxModelMatrix(/*scale=*/1.5f);
  celestial_->UpdateSkyboxData(frame, skybox_transform_matrix);

  aurora_path_->UpdatePerFrameData(
      frame, general_camera, aurora_layer_.model_matrix(), click_aurora_layer);

  // Reset right mouse button flag.
  did_release_right_ = false;
}

void Editor::Draw(const VkCommandBuffer& command_buffer,
                  uint32_t framebuffer_index, int current_frame) {
  absl::optional<int> selected_path_index;
  for (int index = kPath1ButtonIndex; index <= kPath3ButtonIndex; ++index) {
    if (state_manager_.IsSelected(static_cast<ButtonIndex>(index))) {
      selected_path_index = index;
      break;
    }
  }

  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        celestial_->Draw(command_buffer, current_frame);
      },
      [this, current_frame, &selected_path_index](
          const VkCommandBuffer& command_buffer) {
        aurora_path_->Draw(command_buffer, current_frame, selected_path_index);
      },
      [this](const VkCommandBuffer& command_buffer) {
        button_->Draw(command_buffer, state_manager_.button_states());
      },
  });
}

Editor::StateManager::StateManager() {
  button_states_.resize(kNumButtons);
  SetPathButtonStates(Button::State::kHidden);
  button_states_[kEditingButtonIndex] = Button::State::kUnselected;
  button_states_[kDaylightButtonIndex] = Button::State::kUnselected;
  button_states_[kAuroraButtonIndex] = Button::State::kSelected;
}

void Editor::StateManager::Update(absl::optional<ButtonIndex> clicked_button) {
  if (!clicked_button.has_value()) {
    click_info_ = absl::nullopt;
    return;
  }
  const ButtonIndex button_index = clicked_button.value();

  if (click_info_.has_value() && click_info_->button_index == button_index &&
      timer_.GetElapsedTimeSinceLaunch() - click_info_->start_time <
          kButtonBounceTime) {
    return;
  }

  switch (button_index) {
    case kPath1ButtonIndex:
    case kPath2ButtonIndex:
    case kPath3ButtonIndex: {
      if (IsUnselected(button_index)) {
        FlipButtonState(last_edited_path_);
        FlipButtonState(button_index);
        last_edited_path_ = button_index;
      }
      break;
    }
    case kEditingButtonIndex: {
      FlipButtonState(button_index);
      if (IsSelected(button_index)) {
        SetPathButtonStates(Button::State::kUnselected);
        FlipButtonState(last_edited_path_);
      } else {
        SetPathButtonStates(Button::State::kHidden);
      }
      break;
    }
    case kDaylightButtonIndex: {
      FlipButtonState(button_index);
      break;
    }
    case kAuroraButtonIndex: {
      // TODO
      break;
    }
    default:
      FATAL("Unexpected branch");
  }
  click_info_ = ClickInfo{button_index, timer_.GetElapsedTimeSinceLaunch()};
}

int Editor::StateManager::GetEditingPathIndex() const {
  ASSERT_TRUE(IsEditing(), "Not in editing mode");
  for (int i = 0; i < ButtonIndex::kNumAuroraPaths; ++i) {
    const auto button_index =
        static_cast<ButtonIndex>(ButtonIndex::kPath1ButtonIndex + i);
    if (IsSelected(button_index)) {
      return i;
    }
  }
  FATAL("In editing mode but no path is selected. Should never reach here!");
}

void Editor::StateManager::SetPathButtonStates(Button::State state) {
  button_states_[kPath1ButtonIndex] = state;
  button_states_[kPath2ButtonIndex] = state;
  button_states_[kPath3ButtonIndex] = state;
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
} /* namespace jessie_steamer */
