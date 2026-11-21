#ifndef LIGHTER_RENDERER_IR_RENDER_GRAPH_UTIL_H
#define LIGHTER_RENDERER_IR_RENDER_GRAPH_UTIL_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "lighter/renderer/ir/render_graph.h"

namespace lighter::renderer::ir::render_graph {

// Adjacency and liveness root representation extracted from a RenderGraph.
struct PassDependencyGraph {
  // Directed edges for topological sorting (combines RAW/WAW producer
  // dependencies and WAR reader->next-writer ordering edges).
  std::vector<std::vector<uint32_t>> adjacency;

  // Reverse producer dependencies (consumer -> producers), excluding WAR edges
  // so dead readers do not stay alive simply because a live pass overwrites
  // the resource afterwards.
  std::vector<std::vector<uint32_t>> producer_deps;

  // Marks passes that write to external resources or declare side effects.
  std::vector<bool> is_live_root;
};

// Builds RAW/WAW/WAR dependency edges and identifies live roots from passes
// and their resource version metadata.
PassDependencyGraph BuildPassDependencyGraph(
    std::span<const PassNode> passes, std::span<const ImageResourceNode> images,
    std::span<const BufferResourceNode> buffers);

// Runs backward BFS from 'is_live_root' over 'producer_deps' and returns a
// per-pass mask where true means the pass is unreachable (culled).
std::vector<bool> CullDeadPasses(
    std::span<const std::vector<uint32_t>> producer_deps,
    const std::vector<bool>& is_live_root);

// Topologically sorts non-culled passes in 'adjacency' using Kahn's algorithm
// with a min-heap for deterministic declaration-order tie-breaking. Throws on
// cycles among live passes.
std::vector<uint32_t> TopologicalSortLivePasses(
    std::span<const std::vector<uint32_t>> adjacency,
    const std::vector<bool>& pass_culled);

// Computes inclusive [first_pass, last_pass] lifetimes in 'execution_order'
// coordinates for all images and buffers touched by live passes.
void ComputeResourceLifetimes(std::span<const PassNode> passes,
                              std::span<const uint32_t> execution_order,
                              std::vector<ResourceLifetime>* image_lifetimes,
                              std::vector<ResourceLifetime>* buffer_lifetimes);

// Optional backend synchronization note attached to a resource-version -> pass
// edge (or resource -> external finalization when consumer_pass is
// kInvalidResourceId) in ExportGraphviz().
struct GraphvizBarrierNote {
  bool is_image = true;
  uint32_t resource_id = 0;
  uint32_t version = 0;
  uint32_t consumer_pass = kInvalidResourceId;
  std::string label;
};

// Exports a compiled RenderGraph as a Graphviz DOT bipartite DAG showing live
// and culled passes, resource versions, lifetimes, and optional barrier notes.
std::string ExportGraphviz(
    const RenderGraph& graph, const CompiledRenderGraph& compiled,
    std::span<const GraphvizBarrierNote> barrier_notes = {});

// Validates that 'descriptor' is compatible with the attachments and
// depth/stencil operations declared on graphics pass 'pass'.
void ValidateGraphicsPipelineForPass(
    const PassNode& pass, const GraphicsPipelineDescriptor& descriptor);

}  // namespace lighter::renderer::ir::render_graph

#endif  // LIGHTER_RENDERER_IR_RENDER_GRAPH_UTIL_H
