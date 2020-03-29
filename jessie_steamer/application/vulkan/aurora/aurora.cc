//
//  aurora.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/editor.h"
#include "jessie_steamer/application/vulkan/aurora/viewer/viewer.h"
#include "jessie_steamer/application/vulkan/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

enum class Scene { kNone, kEditor, kViewer };

constexpr int kNumFramesInFlight = 2;

class AuroraApp : public Application {
 public:
  explicit AuroraApp(const WindowContext::Config& config);

  // Overrides.
  void MainLoop() override;

 private:
  // Returns the current scene.
  aurora::Scene& GetCurrentScene();

  void TransitionSceneIfNeeded();

  bool HasTransitionedScene() const { return current_scene_ != last_scene_; }

  bool should_quit_ = false;
  int current_frame_ = 0;
  Scene last_scene_ = Scene::kNone;
  Scene current_scene_ = Scene::kEditor;
  common::FrameTimer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<aurora::Editor> editor_;
  std::unique_ptr<aurora::Viewer> viewer_;
};

} /* namespace */

AuroraApp::AuroraApp(const WindowContext::Config& window_config)
    : Application{"Aurora Sketcher", window_config} {
  mutable_window_context()->mutable_window()->RegisterPressKeyCallback(
      common::Window::KeyMap::kEscape, [this]() { should_quit_ = true; });
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);
  editor_ = absl::make_unique<aurora::Editor>(mutable_window_context(),
                                              kNumFramesInFlight);
  viewer_ = absl::make_unique<aurora::Viewer>(
      mutable_window_context(), kNumFramesInFlight,
      editor_->GetAuroraPathVertexBuffers());
}

aurora::Scene& AuroraApp::GetCurrentScene() {
  switch (current_scene_) {
    case Scene::kNone:
      FATAL("Unexpected branch");
    case Scene::kEditor:
      return *editor_;
    case Scene::kViewer:
      return *viewer_;
  }
}

void AuroraApp::TransitionSceneIfNeeded() {
  last_scene_ = current_scene_;
  if (!GetCurrentScene().ShouldTransitionScene()) {
    return;
  }
  switch (current_scene_) {
    case Scene::kNone:
      FATAL("Unexpected branch");
    case Scene::kEditor: {
      viewer_->UpdateAuroraPaths(editor_->viewpoint_position());
      current_scene_ = Scene::kViewer;
      break;
    }
    case Scene::kViewer: {
      current_scene_ = Scene::kEditor;
      break;
    }
  }
}

void AuroraApp::MainLoop() {
  while (!should_quit_ && mutable_window_context()->CheckEvents()) {
    timer_.Tick();

    auto& scene = GetCurrentScene();
    if (HasTransitionedScene()) {
      scene.Recreate();
      scene.OnEnter();
    }

    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(),
        [&scene](int frame) { scene.UpdateData(frame); },
        [this, &scene](const VkCommandBuffer& command_buffer,
                       uint32_t framebuffer_index) {
          scene.Draw(command_buffer, framebuffer_index, current_frame_);
        });

    TransitionSceneIfNeeded();
    // If scene has been transitioned, the new scene will be recreated anyway.
    if (!HasTransitionedScene() &&
        (draw_result.has_value() || window_context().ShouldRecreate())) {
      mutable_window_context()->Recreate();
      scene.Recreate();
    }

    current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
  }
  mutable_window_context()->OnExit();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<AuroraApp>(argc, argv, WindowContext::Config{});
}
