//
//  scene.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_EXAMPLE_AURORA_SCENE_H
#define LIGHTER_EXAMPLE_AURORA_SCENE_H

#include "lighter/renderer/ir/render_graph.h"

namespace lighter::example::aurora {

// Interface of scenes used in the aurora application.
class Scene {
 public:
  virtual ~Scene() = default;

  // Registers input callbacks and resets scene state.
  virtual void OnEnter() = 0;

  // Unregisters input callbacks.
  virtual void OnExit() = 0;

  // Resets cursor/surface-dependent state after swapchain recreation.
  virtual void OnSurfaceRecreated() = 0;

  // Populates 'graph' for rendering one frame using 'frame_slot'.
  virtual void BuildFrame(renderer::ir::RenderGraph& graph, int frame_slot) = 0;

  // Whether the application should transition to another scene.
  virtual bool ShouldTransitionScene() const = 0;
};

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_SCENE_H
