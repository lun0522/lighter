Vulkan Hello Triangle Example
---

Table of Contents
---

  * [0. Introduction](#0-introduction)
  * [1. Construct resources - TriangleApp::TriangleApp()](#1-construct-resources---triangleapptriangleapp)
     * [1.1 Command buffer](#11-command-buffer)
     * [1.2 Vertex buffer](#12-vertex-buffer)
     * [1.3 Push constant](#13-push-constant)
     * [1.4 Graphics pipeline](#14-graphics-pipeline)
     * [1.5 Render pass](#15-render-pass)
  * [2. Recreate swapchain - TriangleApp::Recreate()](#2-recreate-swapchain---triangleapprecreate)
  * [3. Update per-frame data - TriangleApp::UpdateData()](#3-update-per-frame-data---triangleappupdatedata)
  * [4. Main loop - TriangleApp::MainLoop()](#4-main-loop---triangleappmainloop)
  * [5. Appendix - Shader code](#5-appendix---shader-code)
     * [5.1 Vertex shader (triangle.vert)](#51-vertex-shader-trianglevert)
     * [5.2 Fragment shader (triangle.frag)](#52-fragment-shader-trianglefrag)

## 0. Introduction

This is a breakdown of the [code](https://github.com/lun0522/lighter/blob/master/lighter/application/vulkan/triangle.cc)
of triangle scene, which renders a triangle that fades in and out with less than
200 lines of code:

![](https://docs.google.com/uc?id=1zhkhNUTzphiqWez6X0y_RKN7WouT7SCE)

One small experiment can be easily done to illustrate the effect of
multisampling. Find the `main()` function at the bottom, and replace
`return AppMain<TriangleApp>(argc, argv, WindowContext::Config{});` with:

```cpp
return AppMain<TriangleApp>(
    argc, argv, WindowContext::Config{}.disable_multisampling());
```

You can see the jags because multisampling is now disabled. A simple comparison:

![](https://docs.google.com/uc?id=1lcpGEs7S2ade-QwmJBAper4LlafzImuu)

## 1. Construct resources - TriangleApp::TriangleApp()

We will talk about how to setup following resources to render this simple scene:

- Command buffer
- Vertex buffer
- Push constant (for people with OpenGL background, this is kind of a
lightweight uniform buffer)
- Graphics pipeline
- Render pass

Let's first look at some important numbers. We will retrieve several images from
the swapchain that we can render to, hence the number of framebuffers will be
set to `window_context().num_swapchain_images()`. We will do double-buffering
when rendering the scene, hence the constant `kNumFramesInFlight` is set to 2.
`TriangleApp` has a member variable, `current_frame_`, which tells us which
"buffer" are we rendering to.

`TriangleApp` is a subclass of `Application`, whose constructor already
constructs a `WindowContext` that handles the window and swapchain, and holds a
`BasicContext`. `BasicContext` holds the common foundation of Vulkan
applications, such as `VkInstance` and `VkDevice`, and can be accessed by
calling `context()`.

### 1.1 Command buffer

`PerFrameCommand` is a set of command buffers dedicated for onscreen rendering.
Semaphores and fences are handled internally so we don't need to worry about the
synchronization. We only need to inform it that we want to do double-buffering:

```cpp
command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);
```

### 1.2 Vertex buffer

To render the triangle, we only need one mesh with three vertices.
`Vertex3DWithColor` allows us to directly specify the 3D position and RGB values
of each vertex:

```cpp
const std::array<Vertex3DWithColor, 3> vertex_data{
    Vertex3DWithColor{/*pos=*/{0.5f, -0.5f, 0.0f},
                      /*color=*/{1.0f, 0.0f, 0.0f}},
    Vertex3DWithColor{/*pos=*/{0.0f, 0.5f, 0.0f},
                      /*color=*/{0.0f, 0.0f, 1.0f}},
    Vertex3DWithColor{/*pos=*/{-0.5f, -0.5f, 0.0f},
                      /*color=*/{0.0f, 1.0f, 0.0f}},
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
    pipeline::GetVertexAttribute<Vertex3DWithColor>());
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

### 1.4 Graphics pipeline

Setting up a pipeline can be very complicated. The wrapper class
`GraphicsPipeline` already fills it with lots of default settings, and now at
minimum we only need to provide:

1. Vertex input binding descriptions (*i.e.* vertex buffer binding point,
reading stride and update rate) and vertex attributes. The binding descriptions
can be generated by the util function
`pipeline::GetPerVertexBindingDescription()`, and the vertex attributes can be
retrieved from the vertex buffer as we already pass them to its constructor.
2. Pipeline layout, including descriptor layouts and push constant ranges used
in this pipeline. For this simple scene, we don't use descriptors. We only need
to pass the `VkPushConstantRange`.
3. Viewport transform. Since this may change when the window is resized, we need
to set it every time when we build/rebuild the pipeline.
4. Color blending for each color attachment.
5. Which shaders to use.

```cpp
pipeline_builder_ = absl::make_unique<PipelineBuilder>(context());
(*pipeline_builder_)
    .SetPipelineName("Triangle")
    .AddVertexInput(
        kVertexBufferBindingPoint,
        pipeline::GetPerVertexBindingDescription<Vertex3DWithColor>(),
        vertex_buffer_->GetAttributes(/*start_location=*/0))
    .SetPipelineLayout(
        /*descriptor_layouts=*/{},
        {alpha_constant_->MakePerFrameRange(VK_SHADER_STAGE_FRAGMENT_BIT)})
    .SetColorBlend({pipeline::GetColorAlphaBlendState(/*enable_blend=*/true)})
    .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
               common::file::GetVkShaderPath("triangle/triangle.vert"))
    .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
               common::file::GetVkShaderPath("triangle/triangle.frag"));
```

### 1.5 Render pass

Setting up a render pass can be even more complicated. We have several layers of
abstraction to make life easier. In this simple scene, we can simply use the
highest level abstraction:

```cpp
enum SubpassIndex {
  kRenderSubpassIndex = 0,
  kNumSubpasses,
};

render_pass_manager_ = absl::make_unique<OnScreenRenderPassManager>(
    &window_context(),
    NaiveRenderPass::SubpassConfig{
        kNumSubpasses, /*first_transparent_subpass=*/absl::nullopt,
        /*first_overlay_subpass=*/kRenderSubpassIndex});
```

Internally, this render pass manager will manage the swapcahin image,
the multisample image (if multisampling is turned on) and the render pass for
us. See class comments of `NaiveRenderPass` for details about subpass configs.

## 2. Recreate swapchain - TriangleApp::Recreate()

After resources are initialized, the render pass does not yet know which image
it should render to. Later in the main loop, if the window is resized, the
swapchain will be recreated by `WindowContext`, and the render pass will need to
update the rendering target. Since we are using the high level abstraction,
render pass manager, it will handle all of those for us:

```cpp
render_pass_manager_->RecreateRenderPass();
```

After the render pass is recreated, we need to build a new `GraphicsPipeline`:

```cpp
(*pipeline_builder_)
    .SetMultisampling(window_context().sample_count())
    .SetViewport(
        pipeline::GetFullFrameViewport(window_context().frame_size()))
    .SetRenderPass(*render_pass(), kRenderSubpassIndex);
pipeline_ = pipeline_builder_->Build();
```

The old `RenderPass` and `GraphicsPipeline` are automatically destructed.

## 3. Update per-frame data - TriangleApp::UpdateData()

We have a `FrameTimer` to compute the frame rate. We can also use its util
functions to get the elapsed time since the timer is constructed, and set the
alpha channel of the triangle based on it. `PushConstant` will update the host
data, which will be flushed to the device later in the main loop:

```cpp
alpha_constant_->HostData<Alpha>(frame)->value =
    glm::abs(glm::sin(timer_.GetElapsedTimeSinceLaunch()));
```

## 4. Main loop - TriangleApp::MainLoop()

Before entering the main loop, we call `TriangleApp::Recreate()` once to
finalize `RenderPass` and `GraphicsPipeline`. In the loop, we first check
whether there is any user inputs from the window, and informs the timer that we
start rendering a new frame:

```cpp
Recreate();
while (mutable_window_context()->CheckEvents()) {
  timer_.Tick();
```

Note that if the user closes the window,
`mutable_window_context()->CheckEvents()` will return `false`, and the main loop
will end.

Next, we use the command buffer to record the rendering operations to perform at
each subpass:

```cpp
const auto update_data = [this](int frame) { UpdateData(frame); };

const auto draw_result = command_->Run(
    current_frame_, window_context().swapchain(), update_data,
    [this](const VkCommandBuffer& command_buffer,
           uint32_t framebuffer_index) {
      render_pass().Run(command_buffer, framebuffer_index, /*render_ops=*/{
          [this](const VkCommandBuffer& command_buffer) {
            pipeline_->Bind(command_buffer);
            alpha_constant_->Flush(command_buffer, pipeline_->layout(),
                                   current_frame_, /*target_offset=*/0,
                                   VK_SHADER_STAGE_FRAGMENT_BIT);
            vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                                 /*mesh_index=*/0, /*instance_count=*/1);
          },
      });
    });
```

`PerFrameCommand` will retrieve an image from the swapchain passed in, update
the per-frame data with the lambda `update_data`, run `RenderPass`, and return
the rendering result. If everything goes well, `draw_result` will be
`absl::nullopt`. If the swapchain is obsolete, or if the window is resized, we
should recreate a part of resources. Otherwise, we just update `current_frame_`
and proceed to the next iteration:

```cpp
if (draw_result.has_value() || window_context().ShouldRecreate()) {
  mutable_window_context()->Recreate();
  Recreate();
}
current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
```

If the user closes the window and the main loop is about to end, we must call
`mutable_window_context()->OnExit()`, which bridges to `BasicContext::OnExit()`.
It will not return until the device becomes idle and `BasicContext` has cleaned
up static resources (see details in `BasicContext::OnExit()`). After that, it is
safe to destruct `TriangleApp` and all the resources we created.

## 5. Appendix - Shader code

### 5.1 Vertex shader (triangle.vert)

```glsl
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 color;

void main() {
  gl_Position = vec4(in_pos, 1.0);
  color = in_color;
}
```

### 5.2 Fragment shader (triangle.frag)

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