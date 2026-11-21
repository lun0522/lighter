//
//  render_graph_util_test.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/ir/render_graph_util.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace lighter::renderer::ir::render_graph {
namespace {

TEST(RenderGraphUtilTest, CullDeadPassesTracksReachabilityFromRoots) {
  // Producer edges (consumer -> [producers]):
  // 0 <- [], 1 <- [0], 2 <- [0] (dead branch), 3 <- [1] (live root).
  const std::vector<std::vector<uint32_t>> producer_deps = {
      {},
      {0},
      {0},
      {1},
  };
  const std::vector<bool> is_live_root = {false, false, false, true};

  const std::vector<bool> culled = CullDeadPasses(producer_deps, is_live_root);
  ASSERT_EQ(culled.size(), 4u);
  EXPECT_FALSE(culled[0]);
  EXPECT_FALSE(culled[1]);
  EXPECT_TRUE(culled[2]);
  EXPECT_FALSE(culled[3]);
}

TEST(RenderGraphUtilTest,
     TopologicalSortRespectsEdgesAndTieBreaksDeterministically) {
  // Edges: 2 -> 0, 2 -> 1 (with duplicate edge 2 -> 0), pass 3 is culled.
  // Pass 2 must execute first, then 0 before 1 (smaller index first).
  const std::vector<std::vector<uint32_t>> adjacency = {
      {},
      {},
      {1, 0, 0},
      {0},  // culled incoming edge should be ignored
  };
  const std::vector<bool> pass_culled = {false, false, false, true};

  const std::vector<uint32_t> order =
      TopologicalSortLivePasses(adjacency, pass_culled);
  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], 2u);
  EXPECT_EQ(order[1], 0u);
  EXPECT_EQ(order[2], 1u);
}

TEST(RenderGraphUtilTest,
     TopologicalSortIgnoresCulledCycleAndThrowsOnLiveCycle) {
  const std::vector<std::vector<uint32_t>> culled_cycle_adj = {
      {},
      {2},
      {1},
  };
  const std::vector<bool> culled_mask = {false, true, true};
  const std::vector<uint32_t> order =
      TopologicalSortLivePasses(culled_cycle_adj, culled_mask);
  ASSERT_EQ(order.size(), 1u);
  EXPECT_EQ(order[0], 0u);

  const std::vector<bool> live_mask = {false, false, false};
  EXPECT_THROW(TopologicalSortLivePasses(culled_cycle_adj, live_mask),
               std::runtime_error);
}

TEST(RenderGraphUtilTest, ExportGraphvizIncludesLiveCulledAndAnnotations) {
  RenderGraph graph;
  const ImageHandle backbuffer = graph.ImportSwapchainImage(/*window_index=*/0);
  ImageHandle temp;
  graph.AddGraphicsPass(
      "Producer",
      [&](PassBuilder& builder) {
        temp = builder.CreateImage(TransientImageDesc{
            .name = "temp",
            .extent = {64, 64},
            .format = TransientImageFormat::kColorUnorm8,
        });
        temp = builder.ColorAttachment(temp, /*location=*/0);
      },
      [](PassContext&) {});
  graph.AddComputePass(
      "DeadPass",
      [&](PassBuilder& builder) {
        builder.Read(temp, ImageUsage::GetLinearAccessInComputeShaderUsage(
                               AccessType::kReadOnly));
      },
      [](PassContext&) {});
  graph.AddGraphicsPass(
      "Present",
      [&](PassBuilder& builder) {
        builder.Read(temp, ImageUsage::GetSampledInFragmentShaderUsage());
        builder.ColorAttachment(backbuffer, /*location=*/0);
      },
      [](PassContext&) {});

  const CompiledRenderGraph compiled = graph.Compile();
  const GraphvizBarrierNote note{
      .is_image = true,
      .resource_id = temp.id,
      .version = temp.version,
      .consumer_pass = 2,
      .label = "ColorAttachment -> ShaderReadOnly",
  };
  const std::string dot = ExportGraphviz(graph, compiled, std::span{&note, 1});

  EXPECT_NE(dot.find("digraph RenderGraph"), std::string::npos);
  EXPECT_NE(dot.find("Producer\\n(Graphics, step 0)"), std::string::npos);
  EXPECT_NE(dot.find("DeadPass\\n(Compute, CULLED)"), std::string::npos);
  EXPECT_NE(dot.find("Present\\n(Graphics, step 1)"), std::string::npos);
  EXPECT_NE(dot.find("ColorAttachment -> ShaderReadOnly"), std::string::npos);
  EXPECT_NE(dot.find("swapchain_0 final"), std::string::npos);
}

TEST(RenderGraphUtilTest,
     ValidateGraphicsPipelineForPassRejectsMismatchedAttachments) {
  PassNode pass;
  pass.name = "MainPass";
  pass.type = PassType::kGraphics;
  pass.color_attachments.push_back(ColorAttachmentDesc{.location = 0});

  const auto valid_color =
      GraphicsPipelineDescriptor{}.SetName("Color").UseColorAttachment(
          0, std::nullopt);
  EXPECT_NO_THROW(ValidateGraphicsPipelineForPass(pass, valid_color));

  const auto wrong_color_loc = GraphicsPipelineDescriptor{}
                                   .SetName("BadColor")
                                   .UseColorAttachment(1, std::nullopt);
  EXPECT_THROW(ValidateGraphicsPipelineForPass(pass, wrong_color_loc),
               std::runtime_error);

  const auto depth_pipeline =
      GraphicsPipelineDescriptor{}.SetName("Depth").EnableDepthTestAndWrite();
  EXPECT_THROW(ValidateGraphicsPipelineForPass(pass, depth_pipeline),
               std::runtime_error);

  // Depth-only attachment (stencil ops left as kDontCare): depth passes,
  // stencil must be rejected.
  pass.depth_stencil_attachment = DepthStencilAttachmentDesc{
      .depth_load_op = AttachmentLoadOp::kClear,
      .depth_store_op = AttachmentStoreOp::kDontCare,
      .stencil_load_op = AttachmentLoadOp::kDontCare,
      .stencil_store_op = AttachmentStoreOp::kDontCare,
  };
  EXPECT_NO_THROW(ValidateGraphicsPipelineForPass(pass, depth_pipeline));

  const auto stencil_read_pipeline =
      GraphicsPipelineDescriptor{}
          .SetName("StencilRead")
          .EnableStencilTest({.compare_op = CompareOp::kNotEqual,
                              .compare_mask = 0xFF,
                              .write_mask = 0x00,
                              .reference = 1},
                             {});
  EXPECT_THROW(ValidateGraphicsPipelineForPass(pass, stencil_read_pipeline),
               std::runtime_error);

  // Enabling stencil load/store ops on the pass allows stencil testing.
  pass.depth_stencil_attachment->stencil_load_op = AttachmentLoadOp::kClear;
  EXPECT_NO_THROW(ValidateGraphicsPipelineForPass(pass, stencil_read_pipeline));

  const auto stencil_write_pipeline =
      GraphicsPipelineDescriptor{}
          .SetName("StencilWrite")
          .EnableStencilTest({.stencil_and_depth_pass_op = StencilOp::kReplace,
                              .write_mask = 0xFF,
                              .reference = 1},
                             {});
  EXPECT_NO_THROW(
      ValidateGraphicsPipelineForPass(pass, stencil_write_pipeline));

  // Read-only attachment still permits depth/stencil reads, rejects writes.
  pass.depth_stencil_attachment->read_only = true;
  const auto depth_read_pipeline =
      GraphicsPipelineDescriptor{}.SetName("DepthRead").EnableDepthTestOnly();
  EXPECT_NO_THROW(ValidateGraphicsPipelineForPass(pass, depth_read_pipeline));
  EXPECT_NO_THROW(ValidateGraphicsPipelineForPass(pass, stencil_read_pipeline));
  EXPECT_THROW(ValidateGraphicsPipelineForPass(pass, depth_pipeline),
               std::runtime_error);
  EXPECT_THROW(ValidateGraphicsPipelineForPass(pass, stencil_write_pipeline),
               std::runtime_error);
}

}  // namespace
}  // namespace lighter::renderer::ir::render_graph
