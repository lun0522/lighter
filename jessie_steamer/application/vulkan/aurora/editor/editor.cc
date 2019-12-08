//
//  editor.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/editor.h"

#include <array>

#include "third_party/absl/memory/memory.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kAuroraPathSubpassIndex,
  kButtonSubpassIndex,
  kNumSubpasses,
  kNumTransparentSubpasses = kAuroraPathSubpassIndex - kModelSubpassIndex,
  kNumOverlaySubpasses = kButtonSubpassIndex - kAuroraPathSubpassIndex,
};

constexpr float kButtonBounceTime = 0.5f;

// The height of aurora layer is assumed to be at around 100km above the ground.
constexpr float kEarthModelRadius = 1.0f;
constexpr float kEarthRadius = 6378.1f;
constexpr float kAuroraHeight = 100.0f;
constexpr float kAuroraLayerRadius =
    (kEarthRadius + kAuroraHeight) / kEarthRadius * kEarthModelRadius;
const glm::vec3 kEarthCenter{0.0f};

glm::vec3 MakeColor(int r, int g, int b) {
  return glm::vec3{r, g, b} / 255.0f;
}

} /* namespace */

const int Editor::kNumAuroraPaths = 3;

Editor::Editor(const WindowContext& window_context,
               int num_frames_in_flight)
    : original_aspect_ratio_{
          util::GetAspectRatio(window_context.frame_size())},
      earth_{kEarthCenter, kEarthModelRadius} {
  const auto context = window_context.basic_context();

  /* Earth and skybox */
  celestial_ = absl::make_unique<Celestial>(
      context, original_aspect_ratio_, num_frames_in_flight);

  /* Aurora path */
  aurora_path_ = absl::make_unique<AuroraPath>(
      context, kNumAuroraPaths, original_aspect_ratio_, num_frames_in_flight);
  for (int path = 0; path < kNumAuroraPaths; ++path) {
    aurora_path_->UpdatePath(path, path_manager_.control_points(path),
                             path_manager_.spline_points(path));
  }

  /* Button */
  using button::ButtonInfo;
  const ButtonInfo button_info{
      Text::Font::kOstrich, /*font_height=*/100, /*base_y=*/0.25f,
      /*top_y=*/0.75f, /*text_color=*/glm::vec3{1.0f},
      /*button_alphas=*/{1.0f, 0.5f}, /*button_size=*/glm::vec2{0.2f, 0.1f},
      /*button_infos=*/{
          ButtonInfo::Info{
              "Path 1",
              /*colors=*/{MakeColor(241, 196,  15), MakeColor(243, 156,  18)},
              /*center=*/glm::vec2{0.2f, 0.9f},
          },
          ButtonInfo::Info{
              "Path 2",
              /*colors=*/{MakeColor(230, 126,  34), MakeColor(211,  84,   0)},
              /*center=*/glm::vec2{0.5f, 0.9f},
          },
          ButtonInfo::Info{
              "Path 3",
              /*colors=*/{MakeColor(231,  76,  60), MakeColor(192,  57,  43)},
              /*center=*/glm::vec2{0.8f, 0.9f},
          },
          ButtonInfo::Info{
              "Editing",
              /*colors=*/{MakeColor( 52, 152, 219), MakeColor( 41, 128, 185)},
              /*center=*/glm::vec2{0.2f, 0.1f},
          },
          ButtonInfo::Info{
              "Daylight",
              /*colors=*/{MakeColor(155,  89, 182), MakeColor(142,  68, 173)},
              /*center=*/glm::vec2{0.5f, 0.1f},
          },
          ButtonInfo::Info{
              "Aurora",
              /*colors=*/{MakeColor( 46, 204, 113), MakeColor( 39, 174,  96)},
              /*center=*/glm::vec2{0.8f, 0.1f},
          },
      },
  };
  button_ = absl::make_unique<Button>(
      context, original_aspect_ratio_, button_info);

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{0.0f, 0.0f, 3.0f};
  camera_ = absl::make_unique<common::UserControlledCamera>(
      config, common::UserControlledCamera::ControlConfig{},
      original_aspect_ratio_);
  camera_->SetActivity(true);

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
        camera_->DidScroll(y_pos, 10.0f, 60.0f);
      })
      .RegisterMouseButtonCallback([this](bool is_left, bool is_press) {
        is_pressing_left_ = is_press;
      });
}

void Editor::OnExit(common::Window* mutable_window) {
  (*mutable_window)
      .RegisterScrollCallback(nullptr)
      .RegisterMouseButtonCallback(nullptr);
}

void Editor::Recreate(const WindowContext& window_context) {
  /* Camera */
  camera_->SetCursorPos(window_context.window().GetCursorPos());

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
  button_->Update(frame_size, sample_count, *render_pass_, kButtonSubpassIndex);
}

void Editor::UpdateData(const WindowContext& window_context, int frame) {
  absl::optional<glm::vec2> click_ndc;
  if (is_pressing_left_) {
    click_ndc = window_context.window().GetNormalizedCursorPos();
    // When the frame is resized, the viewport is changed to maintain the
    // aspect ratio, hence we need to consider the distortion caused by the
    // viewport change.
    const auto current_aspect_ratio =
        util::GetAspectRatio(window_context.frame_size());
    const float distortion = current_aspect_ratio / original_aspect_ratio_;
    if (distortion > 1.0f) {
      click_ndc->x *= distortion;
    } else {
      click_ndc->y /= distortion;
    }
  }

  absl::optional<StateManager::ButtonIndex> clicked_button;
  if (click_ndc.has_value()) {
    const auto clicked_button_index = button_->GetClickedButtonIndex(
        click_ndc.value(), state_manager_.button_states());
    if (clicked_button_index.has_value()) {
      clicked_button = static_cast<StateManager::ButtonIndex>(
          clicked_button_index.value());
      // If any button is clicked, the earth is not clicked.
      click_ndc = absl::nullopt;
    }
  }
  state_manager_.Update(clicked_button);
  earth_.Update(*camera_, click_ndc);

  const auto earth_texture_index =
      state_manager_.button_state(StateManager::kDaylightButtonIndex) ==
          Button::State::kSelected
          ? Celestial::kEarthDayTextureIndex
          : Celestial::kEarthNightTextureIndex;
  celestial_->UpdateEarthData(frame, *camera_, earth_.model_matrix(),
                              earth_texture_index);
  celestial_->UpdateSkyboxData(frame, *camera_, earth_.GetSkyboxModelMatrix());
  aurora_path_->UpdateTransMatrix(frame, *camera_);
}

void Editor::Render(const VkCommandBuffer& command_buffer,
                    uint32_t framebuffer_index, int current_frame) {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        celestial_->Draw(command_buffer, current_frame);
      },
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        aurora_path_->Draw(command_buffer, current_frame);
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

void Editor::StateManager::Update(
    const absl::optional<ButtonIndex>& clicked_button) {
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
      if (button_states_[button_index] == Button::State::kUnselected) {
        FlipButtonState(last_edited_path_);
        FlipButtonState(button_index);
        last_edited_path_ = button_index;
      }
      break;
    }
    case kEditingButtonIndex: {
      FlipButtonState(button_index);
      if (button_states_[button_index] == Button::State::kSelected) {
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

Editor::PathManager::PathManager() {
  constexpr std::array<float, kNumAuroraPaths> kLatitudes{60.0f, 70.0f, 80.0f};
  constexpr int kLongitudeStep = 45;
  constexpr int kNumControlPoints = 360 / kLongitudeStep;
  constexpr int kMinNumControlPoints = 3;
  constexpr int kMaxNumControlPoints = 20;
  constexpr int kMaxRecursionDepth = 20;
  constexpr float kSplineSmoothness = 1E-2;

  std::vector<glm::vec3> control_points;
  control_points.reserve(kNumControlPoints);
  spline_editors_.reserve(kNumAuroraPaths);
  for (float latitude : kLatitudes) {
    const float latitude_radians = glm::radians(latitude);
    control_points.clear();
    const float sin_latitude = glm::sin(latitude_radians);
    const float cos_latitude = glm::cos(latitude_radians);
    for (int longitude = 0; longitude < 360; longitude += kLongitudeStep) {
      const float longitude_radians =
          glm::radians(static_cast<float>(longitude));
      control_points.emplace_back(glm::vec3{
          glm::cos(longitude_radians) * cos_latitude,
          sin_latitude,
          glm::sin(longitude_radians) * cos_latitude,
      });
    }

    spline_editors_.emplace_back(absl::make_unique<common::SplineEditor>(
        kMinNumControlPoints, kMaxNumControlPoints,
        std::vector<glm::vec3>{control_points},
        common::CatmullRomSpline::GetOnSphereSpline(
            kEarthCenter, kAuroraLayerRadius, kMaxRecursionDepth,
            kSplineSmoothness)));
  }
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
