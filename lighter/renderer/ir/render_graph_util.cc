//
//  render_graph_util.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/ir/render_graph_util.h"

#include <algorithm>
#include <functional>
#include <queue>

#include "lighter/common/util.h"

namespace lighter::renderer::ir::render_graph {
namespace {

template <typename ResourceNode>
using VersionReaders = std::vector<std::vector<std::vector<uint32_t>>>;

template <typename ResourceNode, typename Access>
VersionReaders<ResourceNode> CollectReaders(
    std::span<const PassNode> passes, std::span<const ResourceNode> resources,
    std::vector<Access> PassNode::* accesses_member) {
  VersionReaders<ResourceNode> readers(resources.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    readers[i].resize(resources[i].version_writers.size());
  }
  for (const PassNode& pass : passes) {
    for (const Access& access : pass.*accesses_member) {
      if (!access.is_write) {
        readers[access.handle.id][access.handle.version].push_back(pass.index);
      }
    }
  }
  return readers;
}

template <typename ResourceNode, typename Access, typename AddEdgeFn>
void AddResourceDependencies(const PassNode& pass,
                             std::span<const ResourceNode> resources,
                             const VersionReaders<ResourceNode>& readers,
                             std::vector<Access> PassNode::* accesses,
                             AddEdgeFn&& add_edge,
                             std::vector<bool>* is_live_root) {
  for (const Access& access : pass.*accesses) {
    const ResourceNode& node = resources[access.handle.id];
    const uint32_t version = access.handle.version;
    if (access.is_write) {
      if (node.is_external) {
        (*is_live_root)[pass.index] = true;
      }
      if (version > 0) {
        // WAW edge from previous version writer.
        if (node.version_writers[version - 1] >= 0) {
          add_edge(static_cast<uint32_t>(node.version_writers[version - 1]),
                   pass.index, /*is_producer_dep=*/true);
        }
        // WAR edges from readers of previous version.
        for (uint32_t reader : readers[access.handle.id][version - 1]) {
          add_edge(reader, pass.index, /*is_producer_dep=*/false);
        }
      }
    } else if (node.version_writers[version] >= 0) {
      // RAW edge from the pass that produced this version.
      add_edge(static_cast<uint32_t>(node.version_writers[version]), pass.index,
               /*is_producer_dep=*/true);
    }
  }
}

template <typename Access>
void RecordAccessLifetimes(const PassNode& pass, int step,
                           std::vector<Access> PassNode::* accesses,
                           std::vector<ResourceLifetime>* lifetimes) {
  for (const Access& access : pass.*accesses) {
    ResourceLifetime& lifetime = (*lifetimes)[access.handle.id];
    if (!lifetime.IsUsed()) {
      lifetime.first_pass = step;
    }
    lifetime.last_pass = step;
  }
}

}  // namespace

PassDependencyGraph BuildPassDependencyGraph(
    std::span<const PassNode> passes, std::span<const ImageResourceNode> images,
    std::span<const BufferResourceNode> buffers) {
  const size_t num_passes = passes.size();
  PassDependencyGraph graph{
      .adjacency = std::vector<std::vector<uint32_t>>(num_passes),
      .producer_deps = std::vector<std::vector<uint32_t>>(num_passes),
      .is_live_root = std::vector<bool>(num_passes, false),
  };

  const auto image_readers =
      CollectReaders(passes, images, &PassNode::image_accesses);
  const auto buffer_readers =
      CollectReaders(passes, buffers, &PassNode::buffer_accesses);

  const auto add_edge = [&graph](uint32_t from, uint32_t to,
                                 bool is_producer_dep) {
    if (from == to) {
      return;
    }
    graph.adjacency[from].push_back(to);
    if (is_producer_dep) {
      graph.producer_deps[to].push_back(from);
    }
  };

  for (const PassNode& pass : passes) {
    if (pass.has_side_effect) {
      graph.is_live_root[pass.index] = true;
    }
    AddResourceDependencies(pass, images, image_readers,
                            &PassNode::image_accesses, add_edge,
                            &graph.is_live_root);
    AddResourceDependencies(pass, buffers, buffer_readers,
                            &PassNode::buffer_accesses, add_edge,
                            &graph.is_live_root);
  }
  return graph;
}

std::vector<bool> CullDeadPasses(
    std::span<const std::vector<uint32_t>> producer_deps,
    const std::vector<bool>& is_live_root) {
  const size_t num_passes = producer_deps.size();
  ASSERT_TRUE(is_live_root.size() == num_passes,
              "is_live_root size must match producer_deps size");

  std::vector<bool> pass_culled(num_passes, true);
  std::queue<uint32_t> live_queue;
  for (size_t i = 0; i < num_passes; ++i) {
    if (is_live_root[i]) {
      pass_culled[i] = false;
      live_queue.push(static_cast<uint32_t>(i));
    }
  }
  while (!live_queue.empty()) {
    const uint32_t pass_idx = live_queue.front();
    live_queue.pop();
    for (uint32_t producer : producer_deps[pass_idx]) {
      if (pass_culled[producer]) {
        pass_culled[producer] = false;
        live_queue.push(producer);
      }
    }
  }
  return pass_culled;
}

std::vector<uint32_t> TopologicalSortLivePasses(
    std::span<const std::vector<uint32_t>> adjacency,
    const std::vector<bool>& pass_culled) {
  const size_t num_passes = adjacency.size();
  ASSERT_TRUE(pass_culled.size() == num_passes,
              "pass_culled size must match adjacency size");

  // Deduplicate outgoing edges per live node so in-degrees are exact.
  std::vector<std::vector<uint32_t>> deduped_adj(num_passes);
  std::vector<uint32_t> in_degree(num_passes, 0);
  size_t live_count = 0;
  for (size_t u = 0; u < num_passes; ++u) {
    if (pass_culled[u]) {
      continue;
    }
    ++live_count;
    deduped_adj[u] = adjacency[u];
    std::sort(deduped_adj[u].begin(), deduped_adj[u].end());
    deduped_adj[u].erase(
        std::unique(deduped_adj[u].begin(), deduped_adj[u].end()),
        deduped_adj[u].end());
    for (uint32_t v : deduped_adj[u]) {
      ASSERT_TRUE(v < num_passes, "Adjacency target pass index out of range");
      if (!pass_culled[v]) {
        ++in_degree[v];
      }
    }
  }

  std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>>
      ready;
  for (size_t i = 0; i < num_passes; ++i) {
    if (!pass_culled[i] && in_degree[i] == 0) {
      ready.push(static_cast<uint32_t>(i));
    }
  }

  std::vector<uint32_t> execution_order;
  execution_order.reserve(live_count);
  while (!ready.empty()) {
    const uint32_t u = ready.top();
    ready.pop();
    execution_order.push_back(u);
    for (uint32_t v : deduped_adj[u]) {
      if (!pass_culled[v] && --in_degree[v] == 0) {
        ready.push(v);
      }
    }
  }
  ASSERT_TRUE(execution_order.size() == live_count,
              "Cycle detected in RenderGraph");
  return execution_order;
}

void ComputeResourceLifetimes(std::span<const PassNode> passes,
                              std::span<const uint32_t> execution_order,
                              std::vector<ResourceLifetime>* image_lifetimes,
                              std::vector<ResourceLifetime>* buffer_lifetimes) {
  for (size_t order_idx = 0; order_idx < execution_order.size(); ++order_idx) {
    const int step = static_cast<int>(order_idx);
    const PassNode& pass = passes[execution_order[order_idx]];
    RecordAccessLifetimes(pass, step, &PassNode::image_accesses,
                          image_lifetimes);
    RecordAccessLifetimes(pass, step, &PassNode::buffer_accesses,
                          buffer_lifetimes);
  }
}

}  // namespace lighter::renderer::ir::render_graph
