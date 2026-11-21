//
//  render_graph_util.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/ir/render_graph_util.h"

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <queue>
#include <sstream>
#include <string_view>
#include <tuple>

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

namespace {

std::string EscapeDotLabel(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

using BarrierNoteKey = std::tuple<bool, uint32_t, uint32_t, uint32_t>;

BarrierNoteKey MakeBarrierNoteKey(bool is_image, uint32_t resource_id,
                                  uint32_t version, uint32_t consumer_pass) {
  return std::make_tuple(is_image, resource_id, version, consumer_pass);
}

}  // namespace

std::string ExportGraphviz(const RenderGraph& graph,
                           const CompiledRenderGraph& compiled,
                           std::span<const GraphvizBarrierNote> barrier_notes) {
  std::map<BarrierNoteKey, std::string> note_map;
  for (const GraphvizBarrierNote& note : barrier_notes) {
    const BarrierNoteKey key = MakeBarrierNoteKey(
        note.is_image, note.resource_id, note.version, note.consumer_pass);
    auto& entry = note_map[key];
    if (!entry.empty()) {
      entry += "\n";
    }
    entry += note.label;
  }

  std::vector<int> pass_step(graph.passes().size(), -1);
  for (size_t step = 0; step < compiled.execution_order.size(); ++step) {
    pass_step[compiled.execution_order[step]] = static_cast<int>(step);
  }

  std::ostringstream out;
  out << "digraph RenderGraph {\n";
  out << "  rankdir=LR;\n";
  out << "  node [fontname=\"Helvetica\", fontsize=11];\n";
  out << "  edge [fontname=\"Helvetica\", fontsize=9];\n\n";

  // Emit pass nodes.
  for (const PassNode& pass : graph.passes()) {
    const bool culled = pass.index < compiled.pass_culled.size() &&
                        compiled.pass_culled[pass.index];
    const char* pass_type =
        pass.type == PassType::kGraphics ? "Graphics" : "Compute";
    const char* fill =
        culled ? "#eeeeee"
               : (pass.type == PassType::kGraphics ? "#cfe8ff" : "#d1f2eb");
    const char* style = culled ? "dashed,filled" : "filled";
    const char* fontcolor = culled ? "#888888" : "#111111";

    std::string label =
        std::format("{}\\n({}", EscapeDotLabel(pass.name), pass_type);
    if (culled) {
      label += ", CULLED";
    } else if (pass_step[pass.index] >= 0) {
      label += std::format(", step {}", pass_step[pass.index]);
    }
    if (pass.has_side_effect) {
      label += ", side-effect";
    }
    label += ")";

    out << std::format(
        "  pass_{} [shape=box, style=\"{}\", fillcolor=\"{}\", "
        "fontcolor=\"{}\", label=\"{}\"];\n",
        pass.index, style, fill, fontcolor, label);
  }
  out << "\n";

  std::vector<std::vector<bool>> image_version_read(graph.images().size());
  for (size_t i = 0; i < graph.images().size(); ++i) {
    image_version_read[i].assign(graph.images()[i].version_writers.size(),
                                 false);
  }
  std::vector<std::vector<bool>> buffer_version_read(graph.buffers().size());
  for (size_t i = 0; i < graph.buffers().size(); ++i) {
    buffer_version_read[i].assign(graph.buffers()[i].version_writers.size(),
                                  false);
  }
  for (const PassNode& pass : graph.passes()) {
    for (const ImageAccess& access : pass.image_accesses) {
      if (!access.is_write) {
        image_version_read[access.handle.id][access.handle.version] = true;
      }
    }
    for (const BufferAccess& access : pass.buffer_accesses) {
      if (!access.is_write) {
        buffer_version_read[access.handle.id][access.handle.version] = true;
      }
    }
  }

  const auto emit_resources = [&](bool is_image, const auto& resources,
                                  const auto& lifetimes,
                                  const auto& version_read) {
    const char* prefix = is_image ? "img" : "buf";
    const char* shape = is_image ? "ellipse" : "diamond";
    for (size_t r_id = 0; r_id < resources.size(); ++r_id) {
      const auto& node = resources[r_id];
      const ResourceLifetime lifetime =
          r_id < lifetimes.size() ? lifetimes[r_id] : ResourceLifetime{};
      const bool used = lifetime.IsUsed();
      const bool has_final =
          node.is_external &&
          node.final_usage.usage_type() !=
              decltype(node.final_usage)::UsageType::kDontCare;
      const char* fill =
          !used ? "#f2f2f2" : (node.is_external ? "#fdebd0" : "#d5f5e3");
      const char* style = used ? "filled" : "dashed,filled";
      const char* fontcolor = used ? "#111111" : "#888888";

      std::string lifetime_str =
          used
              ? std::format("[{}, {}]", lifetime.first_pass, lifetime.last_pass)
              : "unused";

      for (uint32_t v = 0; v < node.version_writers.size(); ++v) {
        const int writer = node.version_writers[v];
        const bool needed_for_final = has_final && v == node.current_version;
        if (writer < 0 && !version_read[r_id][v] && !needed_for_final) {
          continue;
        }
        std::string res_label = std::format(
            "{} v{}\\n({}, {})", EscapeDotLabel(node.name), v,
            node.is_external ? "external" : "transient", lifetime_str);
        out << std::format(
            "  {}_{}_v{} [shape={}, style=\"{}\", fillcolor=\"{}\", "
            "fontcolor=\"{}\", label=\"{}\"];\n",
            prefix, r_id, v, shape, style, fill, fontcolor, res_label);

        if (writer >= 0) {
          const bool writer_culled =
              writer < static_cast<int>(compiled.pass_culled.size()) &&
              compiled.pass_culled[writer];
          out << std::format("  pass_{} -> {}_{}_v{} [style={}", writer, prefix,
                             r_id, v, writer_culled ? "dashed" : "solid");
          const BarrierNoteKey key =
              MakeBarrierNoteKey(is_image, static_cast<uint32_t>(r_id), v,
                                 static_cast<uint32_t>(writer));
          const auto note_iter = note_map.find(key);
          if (note_iter != note_map.end()) {
            out << std::format(", label=\"{}\"",
                               EscapeDotLabel(note_iter->second));
          }
          out << "];\n";
        }
      }

      if (has_final) {
        const BarrierNoteKey final_key =
            MakeBarrierNoteKey(is_image, static_cast<uint32_t>(r_id),
                               node.current_version, kInvalidResourceId);
        const auto note_iter = note_map.find(final_key);
        const std::string final_node = std::format("{}_{}_final", prefix, r_id);
        out << std::format(
            "  {} [shape=plaintext, fontcolor=\"#555555\", label=\"{} "
            "final\"];\n",
            final_node, EscapeDotLabel(node.name));
        out << std::format("  {}_{}_v{} -> {}", prefix, r_id,
                           node.current_version, final_node);
        if (note_iter != note_map.end()) {
          out << std::format(" [label=\"{}\"]",
                             EscapeDotLabel(note_iter->second));
        }
        out << ";\n";
      }
    }
  };

  emit_resources(/*is_image=*/true, graph.images(), compiled.image_lifetimes,
                 image_version_read);
  emit_resources(/*is_image=*/false, graph.buffers(), compiled.buffer_lifetimes,
                 buffer_version_read);
  out << "\n";

  // Emit read/consume edges from resource versions to passes.
  const auto emit_access_edges = [&](const PassNode& pass, bool is_image,
                                     const auto& accesses) {
    const char* prefix = is_image ? "img" : "buf";
    const bool culled = pass.index < compiled.pass_culled.size() &&
                        compiled.pass_culled[pass.index];
    for (const auto& access : accesses) {
      if (access.is_write) {
        continue;
      }
      const BarrierNoteKey key = MakeBarrierNoteKey(
          is_image, access.handle.id, access.handle.version, pass.index);
      const auto note_iter = note_map.find(key);
      out << std::format("  {}_{}_v{} -> pass_{} [style={}", prefix,
                         access.handle.id, access.handle.version, pass.index,
                         culled ? "dashed, color=\"#aaaaaa\"" : "solid");
      if (note_iter != note_map.end()) {
        out << std::format(", label=\"{}\"", EscapeDotLabel(note_iter->second));
      }
      out << "];\n";
    }
  };

  for (const PassNode& pass : graph.passes()) {
    emit_access_edges(pass, /*is_image=*/true, pass.image_accesses);
    emit_access_edges(pass, /*is_image=*/false, pass.buffer_accesses);
  }

  out << "}\n";
  return out.str();
}

}  // namespace lighter::renderer::ir::render_graph
