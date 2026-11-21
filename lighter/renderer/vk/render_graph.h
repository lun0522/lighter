#ifndef LIGHTER_RENDERER_VK_RENDER_GRAPH_H
#define LIGHTER_RENDERER_VK_RENDER_GRAPH_H

#include <cstdint>
#include <span>
#include <string>

#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/pipeline_cache.h"
#include "lighter/renderer/vk/swapchain.h"
#include "lighter/renderer/vk/transient_resource_pool.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Compiles and executes ir::RenderGraph instances onto a Vulkan command buffer,
// orchestrating TransientResourcePool, Synchronization2 barrier synthesis,
// Dynamic Rendering / compute dispatches, and PipelineCache lookups.
class RenderGraphExecutor : public WithSharedContext {
 public:
  RenderGraphExecutor(const SharedContext& context, int num_frames_in_flight);

  // This class is neither copyable nor movable.
  RenderGraphExecutor(const RenderGraphExecutor&) = delete;
  RenderGraphExecutor& operator=(const RenderGraphExecutor&) = delete;

  // Compiles 'graph', acquires pooled transient resources for 'frame_index',
  // synthesizes Synchronization2 barriers, and records all live passes into
  // 'cmd'.
  void Execute(intl::CommandBuffer cmd, int frame_index,
               const ir::RenderGraph& graph,
               std::span<const Swapchain* const> swapchains,
               std::span<const uint32_t> acquired_swapchain_image_indices);

  // Returns the Graphviz DOT representation of the most recently executed
  // RenderGraph annotated with synthesized Vulkan barrier transitions.
  const std::string& last_graphviz() const { return last_graphviz_; }

 private:
  TransientResourcePool transient_pool_;
  PipelineCache pipeline_cache_;
  std::string last_graphviz_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_RENDER_GRAPH_H
