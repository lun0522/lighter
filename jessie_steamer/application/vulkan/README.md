Vulkan Hello Triangle Example
---

Table of Contents
---

  * [0. Introduction](#0-introduction)
  * [1. Construct resources - TriangleApp::TriangleApp()](#1-construct-resources---triangleapptriangleapp)
     * [1.1 Command buffer](#11-command-buffer)
     * [1.2 Vertex buffer](#12-vertex-buffer)
     * [1.3 Push constant](#13-push-constant)
     * [1.4 Render pass](#14-render-pass)
     * [1.5 Graphics pipeline](#15-graphics-pipeline)
  * [2. Recreate swapchain - TriangleApp::Recreate()](#2-recreate-swapchain---triangleapprecreate)
  * [3. Update per-frame data - TriangleApp::UpdateData()](#3-update-per-frame-data---triangleappupdatedata)
  * [4. Main loop - TriangleApp::MainLoop()](#4-main-loop---triangleappmainloop)
  * [5. Appendix - Shader code](#5-appendix---shader-code)
     * [5.1 Vertex shader (pure_color.vert)](#51-vertex-shader-pure_colorvert)
     * [5.2 Fragment shader (pure_color.frag)](#52-fragment-shader-pure_colorfrag)

## 0. Introduction

This is a breakdown of the code of [triangle scene](https://github.com/lun0522/jessie-steamer/blob/master/jessie_steamer/application/vulkan/triangle.cc),
which renders a triangle that fades in and out with less than 200 lines of code:

![](https://docs.google.com/uc?id=1zhkhNUTzphiqWez6X0y_RKN7WouT7SCE)

One small experiment can be easily done to illustrate the effect of
multisampling. Find the `main()` function at the bottom, and replace
`return AppMain<TriangleApp>(argc, argv, WindowContext::Config{});` with:

```cpp
const auto config = WindowContext::Config{}.disable_multisampling();
return AppMain<TriangleApp>(argc, argv, config);
```

You can see the jags because multisampling is now disabled. A simple comparison:

![](https://docs.google.com/uc?id=1lcpGEs7S2ade-QwmJBAper4LlafzImuu)

## 1. Construct resources - TriangleApp::TriangleApp()

We will talk about how to setup following resources to render this simple scene:

- Command buffer
- Vertex buffer
- Push constant (for people with OpenGL background, this is kind of a
lightweight uniform buffer)
- Render pass
- Graphics pipeline

Let's first look at some important numbers. We will retrieve several images from
the swapchain that we can render to, hence the number of framebuffers will be
set to `window_context_.num_swapchain_images()`. We will do double-buffering
when rendering the scene, hence the constant `kNumFramesInFlight` is set to 2.
`TriangleApp` has a member variable, `current_frame_`, which tells us which
"buffer" are we rendering to.

`TriangleApp` is a subclass of `Application`, whose constructor already
constructs a `WindowContext` that handles the window and swapchain, and holds a
`BasicContext`. `BasicContext` holds the common foundation of Vulkan
applications, such as `VkInstance` and `VkDevice`. We can access `BasicContext`
by calling `context()` within any subclass of `Application`.

### 1.1 Command buffer

`PerFrameCommand` is a set of command buffers dedicated for on-screen rendering.
Semaphores and fences are handled internally so we don't need to worry about the
synchronization. We only need to inform it that we want to do double-buffering:

```cpp
command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);
```

### 1.2 Vertex buffer

To render the triangle, we only need one mesh with three vertices.
`Vertex3DNoTex` allows us to directly specify the 3D position and RGB values of
each vertex:

```cpp
const std::array<Vertex3DNoTex, 3> vertex_data{
    Vertex3DNoTex{/*pos=*/{ 0.5f, -0.5f, 0.0f}, /*color=*/{1.0f, 0.0f, 0.0f}},
    Vertex3DNoTex{/*pos=*/{ 0.0f,  0.5f, 0.0f}, /*color=*/{0.0f, 0.0f, 1.0f}},
    Vertex3DNoTex{/*pos=*/{-0.5f, -0.5f, 0.0f}, /*color=*/{0.0f, 1.0f, 0.0f}},
};
```

The vertex data of different meshes might be stored in different places on the
host, and they can have different forms (see `PerVertexBuffer` for details). In
this simple scene, we can use `PerVertexBuffer::NoIndicesDataInfo` which is
meant for the data that has no indices. This guides the vertex buffer to gather
data on the host side and copy to the device:

```cpp
const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
    /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
};
```

The device will need to know how to interpret the vertex data, so we also need
to pass vertex attributes (in OpenGL, this is set by `glVertexAttribPointer`).
We can use the util function `pipeline::GetVertexAttribute()` for this purpose:

```cpp
vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
    context(), vertex_data_info,
    pipeline::GetVertexAttribute<Vertex3DNoTex>());
```

### 1.3 Push constant

We want to change the alpha value so that the triangle can fade in and out:

```cpp
// Alignment requirements of Vulkan:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
#define ALIGN_SCALAR(type) alignas(sizeof(type))

struct Alpha {
  ALIGN_SCALAR(float) float value;
};
```

This data is passed to the device by an instance of `PushConstant`. We will
inform its constructor that we will do double-buffering, and the size of data
used for each "buffer", so that it will prepare twice the space on the host:

```cpp
push_constant_ = absl::make_unique<PushConstant>(context(), sizeof(Alpha),
                                                 kNumFramesInFlight);
```

### 1.4 Render pass

Render passes can be very complicated. We provide a naive render pass, which may
contain three kinds of subpasses (see details in
`NaiveRenderPassBuilder::SubpassConfig`). For this simple scene, we only need to
use the last kind of subpass, overlay subpass, which does not use any depth
stencil buffer. We also need to tell the constructor whether the rendering
target will be presented to the screen, and whether we will use multisampling:

```cpp
enum SubpassIndex {
  kTriangleSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kTriangleSubpassIndex,
};

const NaiveRenderPassBuilder::SubpassConfig subpass_config{
    /*use_opaque_subpass=*/false,
    /*num_transparent_subpasses=*/0,
    kNumOverlaySubpasses,
};
render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
    context(), subpass_config,
    /*num_framebuffers=*/window_context_.num_swapchain_images(),
    /*present_to_screen=*/true, window_context_.multisampling_mode());
```

### 1.5 Graphics pipeline

Setting up a pipeline can be very complicated as well. The wrapper class
`Pipeline` already fills it with lots of default settings, and now we only need
to provide:

1. Vertex input binding descriptions (*i.e.* vertex buffer binding point,
reading stride and update rate) and vertex attributes. The binding descriptions
can be generated by the util function
`pipeline::GetPerVertexBindingDescription()`, and the vertex attributes can be
retrieved from the vertex buffer as we already pass them to its constructor.
2. Pipeline layout, including descriptor layouts and push constant ranges used
in this pipeline. For this simple scene, we don't use descriptors. We only need
to pass the `VkPushConstantRange` that we prepared earlier.
3. Which shaders to use.

```cpp
pipeline_builder_ = absl::make_unique<PipelineBuilder>(context());
(*pipeline_builder_)
    .SetName("triangle")
    .AddVertexInput(kVertexBufferBindingPoint,
                    pipeline::GetPerVertexBindingDescription<Vertex3DNoTex>(),
                    vertex_buffer_->GetAttributes(/*start_location=*/0))
    .SetPipelineLayout(
        /*descriptor_layouts=*/{},
        {alpha_constant_->MakePerFrameRange(VK_SHADER_STAGE_FRAGMENT_BIT)})
    .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
               common::file::GetVkShaderPath("pure_color.vert"))
    .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
               common::file::GetVkShaderPath("pure_color.frag"));
```

## 2. Recreate swapchain - TriangleApp::Recreate()

After resources are constructed, the render pass does not yet know which image
it should render to. Later in the main loop, if the window is resized, the
swapchain will be recreated by `WindowContext`, and the render pass will need to
update the rendering target. In either case, we will update `RenderPassBuilder`
with the image that it should render to (note that if multisampling is used, it
should render to a multisample image), and build a new `RenderPass`:

```cpp
render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
    render_pass_builder_->color_attachment_index(),
    [this](int framebuffer_index) -> const Image& {
      return window_context_.swapchain_image(framebuffer_index);
    });
if (render_pass_builder_->has_multisample_attachment()) {
  render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder_->multisample_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context_.multisample_image();
      });
}
render_pass_ = (*render_pass_builder_)->Build();
```

Similarly, some pipeline states may also change in those cases, and we also need
to build a new `Pipeline`. Note that color blend need to be enabled for the
fading in and out effect:

```cpp
(*pipeline_builder_)
    .SetMultisampling(window_context_.sample_count())
    .SetViewport(pipeline::GetFullFrameViewport(window_context_.frame_size()))
    .SetRenderPass(**render_pass_, kTriangleSubpassIndex)
    .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/true)});
pipeline_ = pipeline_builder_->Build();
```

The old `RenderPass` and `Pipeline` are automatically destructed.

## 3. Update per-frame data - TriangleApp::UpdateData()

We have a `Timer` to compute the frame rate. We can also use its util functions
to get the elapsed time since the timer is constructed, and set the alpha
channel of the triangle based on it. `PushConstant` will update the host data,
which will be flushed to the device later in the main loop:

```cpp
const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
push_constant_->HostData<Alpha>(frame)->value =
    glm::abs(glm::sin(elapsed_time));
```

## 4. Main loop - TriangleApp::MainLoop()

Before entering the main loop, we call `TriangleApp::Recreate()` once to
finalize `RenderPass` and `Pipeline`. In the loop, we first check whether there
is any user inputs from the window, and informs the timer that we start
rendering a new frame:

```cpp
Recreate();
while (window_context_.CheckEvents()) {
  timer_.Tick();
......
```

Note that if the user closes the window, `window_context_.CheckEvents()` will
return `false`, and the main loop will end.

Next, we'll define which rendering operations should be performed when the
command buffer is recording commands:

```cpp
const vector<RenderPass::RenderOp> render_ops{
    [&](const VkCommandBuffer& command_buffer) {
      pipeline_->Bind(command_buffer);
      push_constant_->Flush(command_buffer, pipeline_->layout(),
                            current_frame_, /*target_offset=*/0,
                            VK_SHADER_STAGE_FRAGMENT_BIT);
      vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                           /*mesh_index=*/0, /*instance_count=*/1);
    },
};
```

The size of `render_ops` must match with the number of subpasses we have in
`RenderPass`. With this, we have related the vertex buffer, push constant,
render pass and graphics pipeline. The next step is to related them to the last
resource we have created, the command buffer:

```cpp
const auto update_data = [this](int frame) { UpdateData(frame); };

const auto draw_result = command_->Run(
    current_frame_, window_context_.swapchain(), update_data,
    [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
      render_pass_->Run(command_buffer, framebuffer_index, render_ops);
    });
```

`PerFrameCommand` will retrieve an image from the swapchain passed in, update
the per-frame data with the lambda `update_data()`, run `RenderPass`, and return
the rendering result. If everything goes well, `draw_result` will be
`absl::nullopt`. If the swapchain is obsolete, or if the window is resized, we
should recreate a part of resources. Otherwise, we just update `current_frame_`
and proceed to the next iteration:

```cpp
if (draw_result.has_value() || window_context_.ShouldRecreate()) {
  window_context_.Recreate();
  Recreate();
}
current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
```

If the user closes the window and the main loop is about to end, we must call
`window_context_.OnExit()`, which bridges to `BasicContext::OnExit()`. It will
not return until the device becomes idle and `BasicContext` has cleaned up
static resources (see details in `BasicContext::OnExit()`). After that, it is
safe to destruct `TriangleApp` and all the resources we created.

## 5. Appendix - Shader code

### 5.1 Vertex shader (pure_color.vert)

```glsl
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 color;

void main() {
  gl_Position = vec4(in_pos, 1.0);
  color = in_color;
}
```

### 5.2 Fragment shader (pure_color.frag)

```glsl
layout(push_constant) uniform Alpha {
  float value;
} alpha;

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = vec4(color, alpha.value);
}
```