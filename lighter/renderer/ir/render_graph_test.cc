#include "lighter/renderer/ir/render_graph.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace lighter::renderer::ir {
namespace {

TEST(RenderGraphTest, BuildGraphicsPassWithTransientMsaaAndSwapchain) {
  RenderGraph graph;
  const ImageHandle backbuffer = graph.ImportSwapchainImage(/*window_index=*/0);
  EXPECT_TRUE(backbuffer.IsValid());
  EXPECT_EQ(backbuffer.version, 0u);
  EXPECT_TRUE(graph.GetImageResource(backbuffer).is_swapchain());

  ImageHandle msaa;
  ImageHandle resolved;
  graph.AddGraphicsPass(
      "Triangle",
      [&](PassBuilder& builder) {
        msaa = builder.CreateImage(TransientImageDesc{
            .name = "color_multisample",
            .format = TransientImageFormat::kMatchExternalImage,
            .multisampling_mode = MultisamplingMode::kDecent,
            .match_image = backbuffer,
        });
        msaa = builder.ColorAttachment(msaa, /*location=*/0);
        resolved = builder.ResolveAttachment(msaa, backbuffer);
      },
      [](PassContext&) {});

  ASSERT_EQ(graph.passes().size(), 1u);
  const PassNode& pass = graph.passes()[0];
  EXPECT_EQ(pass.type, PassType::kGraphics);
  ASSERT_EQ(pass.color_attachments.size(), 1u);
  EXPECT_EQ(pass.color_attachments[0].image, msaa);
  ASSERT_EQ(pass.resolve_attachments.size(), 1u);
  EXPECT_EQ(pass.resolve_attachments[0].source, msaa);
  EXPECT_EQ(pass.resolve_attachments[0].target, resolved);

  EXPECT_EQ(msaa.version, 1u);
  EXPECT_EQ(resolved.version, 1u);
  EXPECT_EQ(graph.GetImageResource(msaa).version_writers.at(1), 0);
  EXPECT_EQ(graph.GetImageResource(resolved).version_writers.at(1), 0);
}

TEST(RenderGraphTest, MultiPassDeferredAndComputeVersionChain) {
  RenderGraph graph;
  const ImageHandle backbuffer = graph.ImportSwapchainImage(/*window_index=*/0);

  ImageHandle g_pos;
  ImageHandle g_depth;
  graph.AddGraphicsPass(
      "Geometry",
      [&](PassBuilder& builder) {
        g_pos = builder.CreateImage(TransientImageDesc{
            .name = "g_position",
            .extent = {1280, 720},
            .format = TransientImageFormat::kColorFloat16,
        });
        g_depth = builder.CreateImage(TransientImageDesc{
            .name = "g_depth",
            .extent = {1280, 720},
            .format = TransientImageFormat::kDepthStencil,
        });
        g_pos = builder.ColorAttachment(g_pos, /*location=*/0);
        g_depth = builder.DepthAttachment(g_depth);
      },
      [](PassContext&) {});

  ImageHandle filtered;
  graph.AddComputePass(
      "PostBlur",
      [&](PassBuilder& builder) {
        builder.Read(g_pos, ImageUsage::GetLinearAccessInComputeShaderUsage(
                                AccessType::kReadOnly));
        filtered = builder.CreateImage(TransientImageDesc{
            .name = "filtered",
            .extent = {1280, 720},
            .format = TransientImageFormat::kColorUnorm8,
        });
        filtered = builder.Write(
            filtered, ImageUsage::GetLinearAccessInComputeShaderUsage(
                          AccessType::kWriteOnly));
      },
      [](PassContext&) {});

  ImageHandle final_target;
  graph.AddGraphicsPass(
      "Lighting",
      [&](PassBuilder& builder) {
        builder.Read(filtered, ImageUsage::GetSampledInFragmentShaderUsage());
        final_target = builder.ColorAttachment(backbuffer, /*location=*/0);
      },
      [](PassContext&) {});

  ASSERT_EQ(graph.passes().size(), 3u);
  EXPECT_EQ(g_pos.version, 1u);
  EXPECT_EQ(g_depth.version, 1u);
  EXPECT_EQ(filtered.version, 1u);
  EXPECT_EQ(final_target.version, 1u);
  EXPECT_EQ(graph.GetImageResource(filtered).all_usages.size(), 2u);

  graph.Reset();
  EXPECT_TRUE(graph.passes().empty());
  EXPECT_TRUE(graph.images().empty());
  EXPECT_TRUE(graph.buffers().empty());
}

TEST(RenderGraphTest, TransientBufferVersionAndReset) {
  RenderGraph graph;
  BufferHandle storage_buf;
  graph.AddComputePass(
      "WriteBuffer",
      [&](PassBuilder& builder) {
        storage_buf = builder.CreateBuffer(
            TransientBufferDesc{.name = "particles", .size = 4096});
        storage_buf = builder.Write(
            storage_buf,
            BufferUsage::GetUniformBufferUsage(AccessLocation::kComputeShader));
      },
      [](PassContext&) {});
  graph.AddGraphicsPass(
      "ReadBuffer",
      [&](PassBuilder& builder) {
        builder.Read(storage_buf, BufferUsage::GetVertexBufferUsage(
                                      BufferUsage::UsageType::kVertexOnly));
        builder.SetSideEffect();
      },
      [](PassContext&) {});

  ASSERT_EQ(graph.passes().size(), 2u);
  EXPECT_EQ(storage_buf.version, 1u);
  EXPECT_TRUE(graph.passes()[1].has_side_effect);
  EXPECT_EQ(graph.GetBufferResource(storage_buf).version_writers.at(1), 0);
}

TEST(RenderGraphTest, StaleWriteThrows) {
  RenderGraph graph;
  const ImageHandle target = graph.ImportSwapchainImage(/*window_index=*/0);
  graph.AddGraphicsPass(
      "Pass1",
      [&](PassBuilder& builder) {
        builder.ColorAttachment(target, /*location=*/0);
      },
      [](PassContext&) {});
  EXPECT_THROW(graph.AddGraphicsPass(
                   "Pass2",
                   [&](PassBuilder& builder) {
                     builder.ColorAttachment(target, /*location=*/0);
                   },
                   [](PassContext&) {}),
               std::runtime_error);
}

TEST(RenderGraphTest, CompileCullsDeadPassesAndComputesLifetimes) {
  RenderGraph graph;
  const ImageHandle backbuffer = graph.ImportSwapchainImage(/*window_index=*/0);

  ImageHandle g_color;
  ImageHandle unused_aux;
  graph.AddGraphicsPass(
      "Geometry",
      [&](PassBuilder& builder) {
        g_color = builder.CreateImage(TransientImageDesc{
            .name = "g_color",
            .extent = {1280, 720},
            .format = TransientImageFormat::kColorUnorm8,
        });
        g_color = builder.ColorAttachment(g_color, /*location=*/0);
        unused_aux = builder.CreateImage(TransientImageDesc{
            .name = "unused_aux",
            .extent = {1280, 720},
            .format = TransientImageFormat::kColorUnorm8,
        });
      },
      [](PassContext&) {});

  ImageHandle dead_blur;
  graph.AddComputePass(
      "DeadBlur",
      [&](PassBuilder& builder) {
        builder.Read(g_color, ImageUsage::GetLinearAccessInComputeShaderUsage(
                                  AccessType::kReadOnly));
        dead_blur = builder.CreateImage(TransientImageDesc{
            .name = "dead_blur",
            .extent = {1280, 720},
            .format = TransientImageFormat::kColorUnorm8,
        });
        dead_blur = builder.Write(
            dead_blur, ImageUsage::GetLinearAccessInComputeShaderUsage(
                           AccessType::kWriteOnly));
      },
      [](PassContext&) {});

  graph.AddGraphicsPass(
      "Composite",
      [&](PassBuilder& builder) {
        builder.Read(g_color, ImageUsage::GetSampledInFragmentShaderUsage());
        builder.ColorAttachment(backbuffer, /*location=*/0);
      },
      [](PassContext&) {});

  const CompiledRenderGraph compiled = graph.Compile();
  ASSERT_EQ(compiled.execution_order.size(), 2u);
  EXPECT_EQ(compiled.execution_order[0], 0u);  // Geometry
  EXPECT_EQ(compiled.execution_order[1], 2u);  // Composite
  EXPECT_FALSE(compiled.pass_culled[0]);
  EXPECT_TRUE(compiled.pass_culled[1]);  // DeadBlur culled!
  EXPECT_FALSE(compiled.pass_culled[2]);

  EXPECT_EQ(compiled.image_lifetimes[g_color.id].first_pass, 0);
  EXPECT_EQ(compiled.image_lifetimes[g_color.id].last_pass, 1);
  EXPECT_FALSE(compiled.image_lifetimes[unused_aux.id].IsUsed());
  EXPECT_FALSE(compiled.image_lifetimes[dead_blur.id].IsUsed());
}

TEST(RenderGraphTest, CompileSideEffectAndMultiWriteOrdering) {
  RenderGraph graph;
  const ImageHandle backbuffer = graph.ImportSwapchainImage(/*window_index=*/0);

  ImageHandle img;
  graph.AddGraphicsPass(
      "Pass0_Create",
      [&](PassBuilder& builder) {
        img = builder.CreateImage(TransientImageDesc{
            .name = "ping",
            .extent = {64, 64},
            .format = TransientImageFormat::kColorFloat16,
        });
        img = builder.ColorAttachment(img, /*location=*/0);
      },
      [](PassContext&) {});

  const ImageHandle img_v1 = img;
  graph.AddComputePass(
      "Pass1_ReadV1SideEffect",
      [&](PassBuilder& builder) {
        builder.Read(img_v1, ImageUsage::GetLinearAccessInComputeShaderUsage(
                                 AccessType::kReadOnly));
        builder.SetSideEffect();
      },
      [](PassContext&) {});

  graph.AddComputePass(
      "Pass2_DeadReaderV1",
      [&](PassBuilder& builder) {
        builder.Read(img_v1, ImageUsage::GetLinearAccessInComputeShaderUsage(
                                 AccessType::kReadOnly));
      },
      [](PassContext&) {});

  ImageHandle img_v2;
  graph.AddComputePass(
      "Pass3_OverwriteV1ToV2",
      [&](PassBuilder& builder) {
        img_v2 = builder.Write(img_v1,
                               ImageUsage::GetLinearAccessInComputeShaderUsage(
                                   AccessType::kWriteOnly));
      },
      [](PassContext&) {});

  graph.AddGraphicsPass(
      "Pass4_Present",
      [&](PassBuilder& builder) {
        builder.Read(img_v2, ImageUsage::GetSampledInFragmentShaderUsage());
        builder.ColorAttachment(backbuffer, /*location=*/0);
      },
      [](PassContext&) {});

  const CompiledRenderGraph compiled = graph.Compile();
  // DeadReaderV1 (index 2) must stay culled despite WAR edge to Pass3.
  EXPECT_TRUE(compiled.pass_culled[2]);
  ASSERT_EQ(compiled.execution_order.size(), 4u);
  EXPECT_EQ(compiled.execution_order[0], 0u);  // Pass0
  EXPECT_EQ(compiled.execution_order[1], 1u);  // Pass1 (WAR before Pass3)
  EXPECT_EQ(compiled.execution_order[2], 3u);  // Pass3
  EXPECT_EQ(compiled.execution_order[3], 4u);  // Pass4
}

}  // namespace
}  // namespace lighter::renderer::ir
