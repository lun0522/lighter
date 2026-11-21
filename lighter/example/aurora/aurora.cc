#include <memory>

#include "lighter/example/aurora/editor/editor.h"
#include "lighter/example/aurora/viewer/viewer.h"
#include "lighter/scene/scene.h"
#include "lighter/scene/util.h"

namespace lighter::example {
namespace {

using renderer::ir::MultisamplingMode;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;

constexpr int kWindowIndex = 0;
constexpr int kNumFramesInFlight = 2;

LIGHTER_FLAG(bool, start_in_viewer, false,
             "Start directly in the aurora viewer scene");

enum class SceneKind { kNone, kEditor, kViewer };

class AuroraExample {
 public:
  AuroraExample(const glm::ivec2& screen_size,
                MultisamplingMode multisampling_mode)
      : window_{"Aurora Sketcher", screen_size} {
    renderer_ = scene::CreateRenderer("Aurora Sketcher", {&window_});
    editor_ = std::make_unique<aurora::Editor>(window_, *renderer_,
                                               kWindowIndex, multisampling_mode,
                                               kNumFramesInFlight);
    viewer_ = std::make_unique<aurora::Viewer>(window_, *renderer_,
                                               kWindowIndex, multisampling_mode,
                                               kNumFramesInFlight);
    if (FLAGS_start_in_viewer.Get()) {
      viewer_->UpdateAuroraPaths(editor_->viewpoint_position(),
                                 editor_->GetSplinePathViews());
      current_scene_ = SceneKind::kViewer;
    }
  }

  // This class is neither copyable nor movable.
  AuroraExample(const AuroraExample&) = delete;
  AuroraExample& operator=(const AuroraExample&) = delete;

  void MainLoop() {
    RenderGraph graph;
    while (!window_.ShouldQuit()) {
      window_.ProcessUserInputs();
      timer_.Tick();
      if (timer_.frame_rate() != frame_rate_) {
        frame_rate_ = timer_.frame_rate();
        LOG_INFO << "Frame rate: " << frame_rate_;
      }

      scene::Scene& scene = GetCurrentScene();
      if (HasTransitionedScene()) {
        scene.OnEnter();
      }

      graph.Reset();
      scene.BuildFrame(graph, frame_slot_);
      const bool presented = renderer_->RenderFrame(kWindowIndex, graph);
      frame_slot_ = (frame_slot_ + 1) % kNumFramesInFlight;

      TransitionSceneIfNeeded();
      if (!HasTransitionedScene() && !presented) {
        scene.OnSurfaceRecreated();
      }
    }
    renderer_->WaitIdle();
  }

 private:
  scene::Scene& GetCurrentScene() {
    switch (current_scene_) {
      case SceneKind::kNone:
        FATAL("Unreachable");
      case SceneKind::kEditor:
        return *editor_;
      case SceneKind::kViewer:
        return *viewer_;
    }
  }

  void TransitionSceneIfNeeded() {
    last_scene_ = current_scene_;
    if (!GetCurrentScene().ShouldTransitionScene()) {
      return;
    }
    GetCurrentScene().OnExit();
    switch (current_scene_) {
      case SceneKind::kNone:
        FATAL("Unreachable");
      case SceneKind::kEditor:
        renderer_->WaitIdle();
        viewer_->UpdateAuroraPaths(editor_->viewpoint_position(),
                                   editor_->GetSplinePathViews());
        current_scene_ = SceneKind::kViewer;
        break;
      case SceneKind::kViewer:
        current_scene_ = SceneKind::kEditor;
        break;
    }
  }

  bool HasTransitionedScene() const { return current_scene_ != last_scene_; }

  int frame_slot_ = 0;
  int frame_rate_ = 0;
  SceneKind last_scene_ = SceneKind::kNone;
  SceneKind current_scene_ = SceneKind::kEditor;
  base::FrameTimer timer_;
  base::Window window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<aurora::Editor> editor_;
  std::unique_ptr<aurora::Viewer> viewer_;
};

}  // namespace
}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::scene::RunApplication<lighter::example::AuroraExample>(
      argc, argv, glm::ivec2{800, 600},
      lighter::renderer::ir::MultisamplingMode::kDecent);
}
