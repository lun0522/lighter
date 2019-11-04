//
//  aurora.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor.h"
#include "jessie_steamer/application/vulkan/aurora/viewer.h"
#include "jessie_steamer/application/vulkan/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

constexpr int kNumFramesInFlight = 2;

class AuroraApp : public Application {
 public:
  explicit AuroraApp(const WindowContext::Config& config);

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  bool should_quit_ = false;
  int current_frame_ = 0;
  common::Timer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<aurora::Editor> editor_;
};

} /* namespace */

AuroraApp::AuroraApp(const WindowContext::Config& window_config)
    : Application{"Aurora Sketcher", window_config} {
  /* Window */
  window_context_.mutable_window()->RegisterPressKeyCallback(
      common::Window::KeyMap::kEscape, [this]() { should_quit_ = true; });

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Aurora editor */
  editor_ = absl::make_unique<aurora::Editor>(window_context_,
                                              kNumFramesInFlight);
}

void AuroraApp::Recreate() {
  /* Aurora editor */
  editor_->Recreate(window_context_);
}

void AuroraApp::MainLoop() {
  Recreate();
  editor_->OnEnter(window_context_.mutable_window());

  while (!should_quit_ && window_context_.CheckEvents()) {
    timer_.Tick();

    const auto draw_result = command_->Run(
        current_frame_, window_context_.swapchain(),
        [this](int frame) {
          editor_->UpdateData(window_context_.window(), frame);
        },
        [this](const VkCommandBuffer& command_buffer,
               uint32_t framebuffer_index) {
          editor_->Render(command_buffer, framebuffer_index, current_frame_);
        });

    if (draw_result.has_value() || window_context_.ShouldRecreate()) {
      window_context_.Recreate();
      Recreate();
    }
    current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
  }
  window_context_.OnExit();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<AuroraApp>(argc, argv, WindowContext::Config{});
}
