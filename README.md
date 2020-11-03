Lighter
---

A low-level rendering engine.

Note: The codebase is being constantly refactored, and lots of documentations
may become stale. You may still take a look at [examples](#4-applications-lighterapplication)
of this engine.

Table of Contents
---

   * [0. Introduction](#0-introduction)
   * [1. Common modules (lighter/common/)](#1-common-modules-lightercommon)
      * [1.1 Camera (camera)](#11-camera-camera)
      * [1.2 Character library (char_lib)](#12-character-library-char_lib)
      * [1.3 File utils (file)](#13-file-utils-file)
      * [1.4 Model loader (model_loader)](#14-model-loader-model_loader)
      * [1.5 Reference counting (ref_count)](#15-reference-counting-ref_count)
      * [1.6 Rotation manager (rotation)](#16-rotation-manager-rotation)
      * [1.7 Spline editor (spline)](#17-spline-editor-spline)
      * [1.8 Timer (timer)](#18-timer-timer)
      * [1.9 Window manager (window)](#19-window-manager-window)
   * [2. OpenGL backend (lighter/renderer/opengl/)](#2-opengl-backend-lighterrendereropengl)
   * [3. Vulkan backend (lighter/renderer/vulkan/)](#3-vulkan-backend-lighterrenderervulkan)
      * [3.1 Contexts (basic_context, basic_object, validation and window_context)](#31-contexts-basic_context-basic_object-validation-and-window_context)
      * [3.2 Wrappers](#32-wrappers)
         * [3.2.1 Buffer (buffer)](#321-buffer-buffer)
            * [3.2.1.1 StagingBuffer](#3211-stagingbuffer)
            * [3.2.1.2 VertexBuffer](#3212-vertexbuffer)
            * [3.2.1.3 UniformBuffer and PushConstant](#3213-uniformbuffer-and-pushconstant)
         * [3.2.2 Image (image and image_usage)](#322-image-image-and-image_usage)
         * [3.2.3 Descriptor (descriptor)](#323-descriptor-descriptor)
         * [3.2.4 Pipeline (pipeline and pipeline_util)](#324-pipeline-pipeline-and-pipeline_util)
         * [3.2.5 Render pass (render_pass)](#325-render-pass-render_pass)
         * [3.2.6 Swapchain (swapchain)](#326-swapchain-swapchain)
         * [3.2.7 Synchronization (synchronization)](#327-synchronization-synchronization)
         * [3.2.8 Command (command)](#328-command-command)
      * [3.3 Extensions](#33-extensions)
         * [3.3.1 Image usage utils (image_usage_util)](#331-image-usage-utils-image_usage_util)
         * [3.3.2 High-level pass (base_pass, graphics_pass and compute_pass)](#332-high-level-pass-base_pass-graphics_pass-and-compute_pass)
         * [3.3.3 Naive render pass (naive_render_pass)](#333-naive-render-pass-naive_render_pass)
         * [3.3.4 Model renderer (model)](#334-model-renderer-model)
            * [3.3.4.1 Load required resources](#3341-load-required-resources)
            * [3.3.4.2 Bind optional resources](#3342-bind-optional-resources)
            * [3.3.4.3 Build and update model](#3343-build-and-update-model)
         * [3.3.5 Text renderer (text and text_util)](#335-text-renderer-text-and-text_util)
            * [3.3.5.1 CharLoader and TextLoader](#3351-charloader-and-textloader)
            * [3.3.5.2 StaticText and DynamicText](#3352-statictext-and-dynamictext)
   * [4. Applications (lighter/application/)](#4-applications-lighterapplication)
      * [4.1 Triangle scene (triangle)](#41-triangle-scene-triangle)
      * [4.2 Cube scene (cube)](#42-cube-scene-cube)
      * [4.3 Nanosuit scene (nanosuit)](#43-nanosuit-scene-nanosuit)
      * [4.4 Planet and asteroids scene (planet)](#44-planet-and-asteroids-scene-planet)
      * [4.5 Troop scene (troop)](#45-troop-scene-troop)
      * [4.6 Post effect scene (post_effect)](#46-post-effect-scene-post_effect)
      * [4.7 Aurora sketcher](#47-aurora-sketcher)
         * [4.7.1 Editor scene (aurora/editor/)](#471-editor-scene-auroraeditor)
         * [4.7.2 Viewer scene (aurora/viewer/)](#472-viewer-scene-auroraviewer)
   * [5. Acknowledgements](#5-acknowledgements)
      * [5.1 Tutorials](#51-tutorials)
      * [5.2 Dependencies](#52-dependencies)
      * [5.3 Resources](#53-resources)

# 0. Introduction

This project aims to build a low-level graphics engine with **reusable**
infrastructures, with either OpenGL or Vulkan as backend. It should be able to
run on MacOS. We will add support for Linux later. The code follows
[Google C++ style guide](https://google.github.io/styleguide/cppguide.html) and
only uses the features of C++11 (enhanced by [Abseil library](https://abseil.io)).

To run applications, following these steps:

1. Compile shaders by executing [compile_shaders.sh](https://github.com/lun0522/lighter/blob/master/compile_shaders.sh)
(no command line arguments needed).
2. Install [Bazel build system](https://bazel.build) following the [official guide](https://docs.bazel.build/versions/master/install.html).
3. Run from command line:

```bash
sudo apt install mesa-common-dev  # Only needed for Linux
bazel run -c opt --copt=-DUSE_VULKAN //lighter/application/vulkan:triangle
```

The compilation mode is specified with the `-c` flag. See details on [Bazel website](https://docs.bazel.build/versions/master/command-line-reference.html#flag--compilation_mode).
We do turn on the verbose logging mode if not compiled with `-c opt`. You can
also just build it with Bazel and directly launch the executable, which is more
useful for debugging with external tools:

```bash
bazel build -c dbg --copt=-DUSE_VULKAN //lighter/application/vulkan:triangle
bazel-bin/lighter/application/vulkan/triangle --resource_folder=<path to resource folder> --shader_folder=<path to shader folder> --vulkan_folder=<path to Vulkan SDK folder>
```

Here is the meaning of each flag (note that these are **not** required if you 
directly use `bazel run`):

- *resource_folder*: Since resource files are stored in a separate
[resource repo](https://github.com/lun0522/resource), it must have been
downloaded, and the path to it should be specified with this flag.
- *shader_folder*: The local address of [this folder](https://github.com/lun0522/lighter/tree/master/lighter/shader)
should be specified with this flag.
- *vulkan_folder*: The [Vulkan SDK](https://vulkan.lunarg.com) must have been 
downloaded, and the path to the platform specific folder should be specified
with this flag. For MacOS, this would be
`vulkansdk-macos-<version number>/macOS`.

This README introduces the modules we created, and the decisions we made when we
designed them. The usage of classes and util functions is put right before the
declaration in header files. You can start with a
[Vulkan "Hello Triangle" example](https://github.com/lun0522/lighter/tree/master/lighter/application/vulkan),
and then take a look at other applications under the lighter/application
folder, which are good examples of how to use other features such as rendering
models, skybox and text.

# 1. Common modules (lighter/common/)

These modules are shared by OpenGL and Vulkan backend. Most of the code is
independent of graphics APIs. For those exceptions, we have added preprocessors,
`USE_OPENGL` and `USE_VULKAN`, to make sure if we are not compiling for a
certain API, we won't need to link to anything related to it.

## 1.1 Camera (camera)

![](https://docs.google.com/uc?id=1r543NwYDuXDHqI6TQ6eGpXCbuO-gX8Ya)

We model orthographic cameras and perspective cameras. They don't care about
where the control signal comes from.

- For onscreen applications where we have user interactions, we should create a
**Camera** and pass it to **UserControlledCamera**, which can respond to inputs
from cursor, scroll and keyboard.
- For offscreen rendering, the control signal may come from other sources
instead, such as a log file. We don't have such a demo for now.

## 1.2 Character library (char_lib)

**CharLib** is backed by [FreeType library](https://www.freetype.org). It simply
loads textures of individual characters. We have higher level abstractions to
render texts with characters loaded by it, so the user may not need to directly
use it.

## 1.3 File utils (file)

These util classes are used to load raw data or images from files, and load
vertex data from Wavefront .obj files. The .obj file loader is pretty
lightweight, and it can only load a single mesh, which is useful enough for
simple 3D models like cubes and spheres.

## 1.4 Model loader (model_loader)

**ModelLoader** is backed by [Assimp library](http://www.assimp.org) and is used
to load complex 3D models. The vertex data and textures of each mesh will be
gathered in **ModelLoader::MeshData**. We have higher level abstractions of it
to render models loaded by it, so the user may not need to directly use it.

## 1.5 Reference counting (ref_count)

Smart pointers are good enough for managing resources on the host side in this
project, but we still need something else to manage resources on the device,
especially if they are loaded from files. For example, we might have loaded some
textures for rendering a skybox, and later we might want to use the same
textures to compute reflections when rendering other objects. To avoid
repeatedly loading the same textures from disk to CPU and then to GPU, one
option is to have a shared pointer to reference to the textures, and pass the
pointer around. We think this is not very convenient, and we may not know which
textures we have already loaded, especially when some textures are loaded by the
model loader and we never type in the file path by ourselves.

Another option is to put the pointer in a hashmap, and create an identifier for
it. We can use the file path as the identifier in this case, so that we won't
load a texture twice from the same path, as long as it is still in the hashmap.
We generalize this idea to create this reference counting class, so that we can
do the same thing for other resources, such as shader files. All reference
counted objects of the same class will be put in one hashmap, and will be erased
from it if the reference count drops to zero.

Shader files are a bit different, since after we construct a program in OpenGL,
or a pipeline in Vulkan, we can already destroy them. Even if the next program
or pipeline to build may use the same shaders, their reference counts may have
already dropped to zero before that. Hence, we introduced the auto release pool
to prevent any resource from being destroyed before it goes out of scope. The
user may create an instance of it to preserve loaded shader files, then
construct all models and pipelines. When the instance goes out of scope, shader
files will be released if unused anymore. This is similar to using
`std::lock_guard`.

## 1.6 Rotation manager (rotation)

**RotationManager** class is used to compute how much an object should rotate
following user inputs. **Sphere** uses this manager to handle user interactions
with a sphere, which can have arbitrary center and radius.

The window manager only tells us at each frame, whether the user is clicking on
the screen or not, and the location of the click. It does not tell us whether
that is a single tap or a long press. Hence, **RotationManager** analyzes those
clicks across frames and make reactions, so that:

- For a single tap, no rotation happens.
- For a long press, the object exactly follows the cursor. For example, if we
press on the north pole of an earth model and move our finger, the north pole
will always be under the finger as long as we keep pressing.
- After a long press ends, the object will slow down gradually until it finally
stops, or if a new long press begins.

## 1.7 Spline editor (spline)

![](https://docs.google.com/uc?id=1h8YQZsz9oaYMsfKG5GPtfoEGu4C37GBx)

Splines are represented as a series of points on the spline. We implemented an
algorithm to turn Bézier spline control points into spline points. Other spline
classes, such as **CatmullRomSpline**, do not need to implement this conversion,
but convert their control points to Bézier spline control points, and reuse the
algorithm.

## 1.8 Timer (timer)

![](https://docs.google.com/uc?id=1s-b3fE_-qdEVqM0OOQnckBSATrAojFHs)

**BasicTimer** mainly tracks how much time has elapsed. **FrameTimer** extends
it to track the frame rate, updated every second.

## 1.9 Window manager (window)

**Window** is backed by [GLFW library](https://www.glfw.org). The user can
register callbacks for cursor, scroll and keyboard inputs, and check whether
the window is resized or closed in the main loop. For Vulkan applications, it is
also responsible for providing the names of required extensions and help create
**VkSurfaceKHR**.

# 2. OpenGL backend (lighter/renderer/opengl/)

For now, we only have a minimal example of using OpenGL in this project. This is
expected to be extended in the future.

# 3. Vulkan backend (lighter/renderer/vulkan/)

We don't aim to implement OpenGL APIs on the top of Vulkan APIs, but we do try
to create different levels of abstractions to make life easier. Classes are
grouped into two categories:

- Wrappers, which provide C++ object oriented interfaces for Vulkan objects.
- Extensions, which provide high-level abstractions, built on the top of
wrappers. Our ultimate goal is to have a layer above various graphics APIs, so
that we just need to provide high-level descriptions about what we are trying to
do, and it will lower descriptions to calls to these extensions.

## 3.1 Contexts (basic_context, basic_object, validation and window_context)

This project was first built following the
[Vulkan tutorial by Alexander Overvoorde](https://vulkan-tutorial.com). It is an
amazing tutorial, but it has also sacrificed something to be an amazing one. It
is very understandable that the author put almost all the code in one source
file, without encapsulating components into different classes, however, this
style is not good for building reusable infrastructures. We need to have
hierarchies, inheritances, interfaces and so on, hence we need to make different
design decisions.

The first decision we had to make was about providing a context. Many Vulkan
functions need access to **VkInstance**, **VkDevice** and many other Vulkan
objects. A lot of them are shared when we render different targets, which makes
them the "context". The original design of this project was to have a wrapper
for each of these Vulkan objects, and use one monolithic context class to hold
instances of all wrappers, including **Instance**, **Command** and **Pipeline**,
etc. That made it easier to follow tutorials, but problems revealed as the
project got bigger:

1. It was convenient to pass a context to a function so that it can fetch
everything it needs, but this made it harder to know which part of the context
was actually used. For example, in the past when we [built a pipeline](https://github.com/lun0522/lighter/blob/1f307a02b25e5f7957b173b96f244ead6cbae53a/lighter/renderer/vulkan/pipeline.cc#L47)
we passed in the entire context but only touched **Device**,
**HostMemoryAllocator** and **Swapchain**. Among them, **Swapchain** was
actually used only once. It makes more sense to only keep those frequently used
members (like **Instance** and **Device**) in the context, and pass others (like
**Swapchain**) as arguments (not to mention **Swapchain** is not used for
offscreen rendering), so that we can more easily know what does the function do
internally.
2. It was very easy to create circular dependencies. Each wrapper class needs
the context to initialize itself and perform other operations, and the context
needs to hold instances of wrappers. This could be solved by using forward
declarations and pointers, but things got worse when wrappers classes need to
depend on each other. It also made it ugly to write the BUILD file for Bazel,
since the circular dependency is prohibited in BUILD files and thus we had to
to put all source files in one target. That means, all of them become visible
to the user, however, we actually don't want users to use some classes directly.
3. It made it harder to extend the program to render more complicated scenes.
For example, it was fine to put one **Swapchain** and one **Pipeline** in the
context if we were only going to render one cube, however, if we want to further
render some text on the frame, the **Swapchain** should be shared, but we would
need more **Pipeline**s. Besides, when we prepare the texture for the text, we
only need to make one render call, but when we want to display it on the screen,
we need to make a render call every frame, hence we need different **Command**s
in different classes.
4. Some members are not necessary for offscreen rendering, such as **Surface**,
**Swapchain** and **Window**. Besides, we should not depend on GLFW in the code
and BUILD file. When we do onscreen rendering, if the window is resized, we need
to recreate the resources that are affected by the frame size, such as
**Swapchain** and **RenderPass**, but most members of the context are actually
not changed. It is possible to decouple the part that is affected by the window.

The final choice is to create a basic context with minimal number of members:

- **HostMemoryAllocator**
- **Instance**
- **DebugCallback** (only in debug mode)
- **PhysicalDevice**
- **Device**
- **Queues**

They are truly shared throughout the entire program. Since these members are not
defined in the same file as the context, we still need to use forward
declarations, but we no longer need to do the same for other wrappers like
**Swapchain** and **Pipeline**. When wrappers need to interact with each other,
we would pass the original Vulkan objects as much as we can, so that wrappers
don't need to depend on each other. For onscreen rendering, we created a window
context, which holds a basic context and other members related to the window:

- **Window**
- **Surface**
- **Swapchain**

We also created a Bazel target `//lighter/renderer/vulkan/extension:onscreen`
which exposes only the files needed for onscreen rendering, so applications only
need to depend on this target and hold one instance of **WindowContext**.

Another decision we made was to create and destroy Vulkan objects in a more RAII
way. The tutorial does not wrap Vulkan objects that much. Each of those objects
is created by a function and destroyed by another, where the destruction
functions are called in the reversed order of construction. However, we found
that some of these orders are not necessary. For example, render passes and
pipelines are independent, and they can be created/destroyed in arbitrary order.
Hence, we extracted objects that should be dealt with carefully, and put them in
**BasicContext** and **WindowContext**, where we rely on the declaration order
(which also determines the destruction order in nature) to make sure they are
created/destroyed properly. Other wrapper objects that are not in the contexts
can be created/destroyed in arbitrary order, so the user need not worry about it
at all.

We need to pay extra attention to the swapcahin recreation, which may happen if
the window is resized or moved to another monitor. Some objects will be
affected, such as **Swapchain** (since the size of swapchain images changes),
**Pipeline** (since the viewport changes) and **RenderPass** (since we will
render to a different set of swapchain images). One solution is to follow the
tutorial and have methods like `Create()` and `Cleanup()` in those wrappers, so
that we can destroy expired resources and recreate them. However, the user of
them would need to worry about when and in what order to call these methods. If
an exception is thrown between calling `Cleanup()` and `Create()`, or if the
user calls `Cleanup()` and forgets to call `Create()`, the object will be left
in an uninitialized state, and the destructor of it would cause more trouble
since it tries to destroy uninitialized Vulkan objects, unless we make it more
complicated by adding an extra boolean to track whether it is initialized. To
make life easier, we decided to make each wrapper have **at most** one method
that need to be called during the swapchain recreation:

- Nothing in **BasicContext** will change.
- **Swapchain** actually has no states that should persist, so we simply destroy
the old instance and create a new one. The user would not need to directly
manage instances of **Swapchain**, since **WindowContext** takes care of it. The
user only need to call `WindowContext::Recreate()`.
- **PipelineBuilder** stores the information that may persist for a pipeline. We
just need to update a part of the information and rebuild **Pipeline**. Since
**Pipeline** is usually managed by higher level wrappers (**Model** and
**Text**), the user only need to call `Model::Update()` and `Text::Update()`.
- Similar to **PipelineBuilder**, we also have **RenderPassBuilder**, with which
we can update a part of the information and rebuild **RenderPass**.

**Surface** is a bit different. Since it is owned by **WindowContext** but
initialized by **BasicContext**, it has an `Init()` method that need to be
called after construction, and we need to use an extra boolean to track whether
it has been initialized. The good thing is, this is handled by
**WindowContext**, so the user would not need to worry about it.

## 3.2 Wrappers

### 3.2.1 Buffer (buffer)

![](https://docs.google.com/uc?id=1uavjqOdzvLh2vD7SLB67eQWT2qySG6lA)

(**PushConstant** is not shown in the inheritance graph, since it does not own
an underlying buffer object.)

#### 3.2.1.1 StagingBuffer

Staging buffer is used to transfer data from the host to buffers that are only
visible to the device. It is meant to be used when constructing other types of
buffers. The user may not directly use it.

#### 3.2.1.2 VertexBuffer

**StaticPerVertexBuffer** and **StaticPerInstanceBuffer** are more commonly use,
which store vertex data that does not change once constructed.

**DynamicPerVertexBuffer** and **DynamicPerInstanceBuffer** provide buffers to
which we can send different data of different size before each render call. The
buffer tracks the current size of itself. If we try to send data whose size is
larger than the current buffer size, it will create a new buffer internally.
Otherwise, it reuses the old buffer.

Dynamic vertex buffers are usually managed by extension classes, such as model
renderer and text renderer. For example, when we want to display the frame rate
per-second, we need to render one rectangle for each digit, but the digits and
the number of digits may always change, so we can take advantage of this buffer.

#### 3.2.1.3 UniformBuffer and PushConstant

Both of them are used to pass uniform data. When constructed, both of them will
allocate memory on both host and device. Before rendering a frame, the user
should acquire a pointer to the host data, populate it and flush to the device.

According to the [Vulkan specification](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap36.html#limits-minmax),
to be compatible of all devices, we only allow **PushConstant** to have at most
128 bytes for each frame. Besides, since it is possible to push constant to one
graphics pipeline using multiple instances of **PushConstant**, we also check
the total size in **Pipeline** to make sure it is still no more than 128 bytes.

### 3.2.2 Image (image and image_usage)

![](https://docs.google.com/uc?id=1Wten70a7BXa0jwJuvR60z6SGcNfGcb0z)

**Image** provides all information we need to set up an attachment in the render
pass. The interface **SamplableImage** is inherited by images that might be
sampled in shaders. The following classes hold image buffers internally:

- **OffscreenImage** creates an image that can be used as the offscreen
rendering target or a storage image.
- **TextureImage** stores an image copied from the host.
- **DepthStencilImage** creates an image that can be used as single-sample depth
stencil attachment.
- **MultisampleImage** can be created as a color attachment or depth stencil
attachment.

There are another three special types of images:

- **SwapchainImage** wraps an image retrieved from the swapchain.
- **SharedTexture** retrieves a **TextureImage** from the reference counted
resource pool. Only if there is no such image alive, it will create a new image.
- **UnownedOffscreenTexture** takes in a const pointer to an offscreen image.
It does not own the image, hence the user is responsible for keeping the
existence of the image.

**image::Usage** describes the usage from three perspectives:

- Usage type (render target, depth stencil, presentation, etc)
- Access type (read-only, write-only, or both)
- Access location (host, compute shader, fragment shader, etc)

This is used to determine which **VkImageUsageFlags** should be used when
creating the image. It is also used to build high-level passes, which will be
introduced later.

### 3.2.3 Descriptor (descriptor)

![](https://docs.google.com/uc?id=1rS8pAF8W6Vsd4AS08rIrBRVdZ8INtqnr)

- **StaticDescriptor** is meant for descriptors that are only updated once
during the command buffer recording. It is supported on all hardware.
- **DynamicDescriptor** is meant for descriptors that are updated multiple times
during the command buffer recording. It requires the extension
*VK_KHR_push_descriptor*, hence it may not be available on all hardware.

They are usually managed by extension classes, such as model renderer and text
renderer. The user would still need to inform the model renderer of the
resources declared in the customized shaders. In extension classes, we choose
which kind of **Descriptor** to use based on the lifetime and update frequency
of the descriptor. For example:

- For the model renderer, since textures used for each mesh keep unchanged
across frames, we use **StaticDescriptor** so that we only need to update it
once at the beginning.
- For the dynamic text renderer, when we render all used characters to a large
texture, we cannot know how many character textures to bind in the shader code.
Besides, we only need to create this large texture once, so the lifetime of this
descriptor is very short. Hence, we use **DynamicDescriptor** to bind the
character textures one at a time.

### 3.2.4 Pipeline (pipeline and pipeline_util)

![](https://docs.google.com/uc?id=1bDVu0Dg5Up1H-qGhjxNOfEx191Fhtbv-)

For graphics pipeline and compute pipeline, we provide a builder class
respectively. If any pipeline state is changed, the user is expected to discard
the old pipeline, but reuse the builder to update those changed states, and then
rebuild the pipeline. For example, after the window is resized, the user may
need to create a new pipeline with updated render pass and viewport.

Through **GraphicsPipelineBuilder** the user may set up:

- Depth and stencil testing
- Front face direction
- Multisampling
- Vertex input bindings and attributes
- Descriptor set layouts
- Push constant ranges
- Viewport and scissor
- Render pass and subpass
- Color blending
- Shaders used in the pipeline

We provide helper functions in *pipeline_util* for setting up the viewport,
color blending and vertex input bindings and attributes.

**ComputePipelineBuilder** can be used to set up:

- Descriptor set layouts
- Push constant ranges
- Shaders used in the pipeline

### 3.2.5 Render pass (render_pass)

We provide a builder class to create instances of **RenderPass**. Through
**RenderPassBuilder**, the user may set:

- Number of framebuffers
- Attachments and associated images
- Multisample attachment resolving
- Subpasses and dependencies between them

When used to build **RenderPass**, the internal states of the builder would be
preserved, hence later if any state is changed, the user can discard the old
**RenderPass**, update the builder and rebuild a new one. For example, when the
window is resized, we will render to a different set of swapchain images, and
the render pass would need to be rebuilt.

### 3.2.6 Swapchain (swapchain)

**Swapchain** holds wrappers of images retrieved from the swapchain. If
constructed with multisampling enabled, it will also hold a multisample color
image. This class is managed by **WindowContext**, hence the user would not need
to directly interact with it. See more details in how we designed the
[window context](https://github.com/lun0522/lighter#31-contexts-basic_context-basic_object-validation-and-window_context).

### 3.2.7 Synchronization (synchronization)

**Semaphores** are used for GPU->GPU sync, while **Fences** are used for
GPU->CPU sync. **PerFrameCommand** uses them to coordinate command buffer
recordings and submissions. The user would not need to directly use them for
rendering simple scenes.

### 3.2.8 Command (command)

![](https://docs.google.com/uc?id=1KWG8cXKi86yj45POchTLWRe69uK1am9f)

- **OneTimeCommand** is used for commands that will be executed only once. For
example, we only need to transfer data to the static vertex buffer once.
- **PerFrameCommand** is used for commands that will be executed every frame.
This is used for onscreen rendering, and it handles the synchronization
internally, so that the user won't need to use semaphores and fences directly.

Both of them are meant to be used directly by the user. We will add more command
classes for offscreen rendering and other use cases, and always shield the
complexity of synchronization from the user.

## 3.3 Extensions

### 3.3.1 Image usage utils (image_usage_util)

- **image::UsageTracker** tracks the current usage of multiple images. We pass a
tracker to a high-level pass, so that the pass builder can look up the usages
prior to that pass, and update the usages once that pass ends.
- **image::UsageHistory** specifies the image usage before and after a high
level pass, and the usage at each subpass during that pass. See introduction to
high-level passes for why do we need to provide this history.

### 3.3.2 High-level pass (base_pass, graphics_pass and compute_pass)

Vulkan requires the application to specify which image layout transitions need
to be performed, and how to synchronize accesses to the backing memory. Back to
the time when we only had graphics applications, building a render pass was
already difficult, so we created [**NaiveRenderPassBuilder**](https://github.com/lun0522/lighter/blob/574edecf5641c6a1fba2755b6c11c0ce0cdeb9ee/lighter/renderer/vulkan/wrapper/render_pass_util.h#L28)
to make life easier. However, it was not elegant to extend that class. For
example:

- Sometimes we want the color attachment to have a different layout after the
render pass, so we added the enum **ColorAttachmentFinalUsage** to guide this
transition. Suppose that one day we want to transition the layout of depth
stencil attachment as well, we would have to change the interface again.
- By default, we don't care about the content stored in depth stencil attachment
prior to this render pass. When we want to preserve the previous content, we
have to pass a boolean to tell this builder. Suppose that we don't want to use
the default settings for other attachments, we would have to change the
interface as well.

When we were creating a scene that used deferred shading, the naive builder
couldn't handle it, so we had to introduce
[**DeferredShadingRenderPassBuilder**](https://github.com/lun0522/lighter/blob/574edecf5641c6a1fba2755b6c11c0ce0cdeb9ee/lighter/renderer/vulkan/wrapper/render_pass_util.h#L100),
which was repeating some code of **NaiveRenderPassBuilder**, and was hard to
extend as well.

When we were adding applications that used compute shaders, things got even more
complicated, since we no longer have common patterns. If we don't have high
level abstractions, users would need to insert memory barriers with Vulkan flags
by themselves.

So, eventually we built **image::Usage**, which is used to infer
**VkImageUsageFlag**, **VkAccessFlags**, **VkPipelineStageFlags** and
**VkImageLayout**. High-level passes are then built on the top of it. The basic
idea is the user would use **image::UsageHistory** to specify the usage of each
image at each subpass, which is then used to construct high-level passes:

- **GraphicsPass** is equivalent to a render pass, and each subpass of it
corresponds to a subpass of the render pass. It finally creates a render pass
builder, and sets attachments, subpasses and subpass dependencies internally, so
that the user only needs to update this builder with actual attachment images.
- **ComputePass** is a sequence of compute shader invocations, where we may need
to insert memory barriers for image layout transitions in between.

### 3.3.3 Naive render pass (naive_render_pass)

Most of render passes used in our applications have a common pattern. They
usually contain three kinds of subpasses:

1. Subpasses for rendering opaque objects. The depth stencil attachment should
be both readable and writable.
2. Subpasses for rendering transparent objects. The depth stencil attachment
should be read-only.
3. Subpasses for rendering overlays, such as text. The depth stencil attachment
is not used.

We built a new **NaiveRenderPass** on the top of **GraphicsPass**. The user only
needs to specify how many subpasses of each kind are used (may be zero). This
class will create **image::UsageHistory** for color, depth stencil and
multisample attachments internally, and use **GraphicsPass** to create the
render pass builder. This helped simplify the application code a lot.

### 3.3.4 Model renderer (model)

**Model** is built to shield a great deal of low-level details from the user, so
that a few lines of code should be enough for displaying a model on the screen.
We will illustrate how to use **ModelBuilder** and **Model**.

#### 3.3.4.1 Load required resources

The user need to specify how to load the model vertex data and textures when
constructing **ModelBuilder**:

- **SingleMeshResource** is meant for loading a single mesh, such as a cube or
sphere. We will need to specify which file contains the vertex data (only
Wavefront .obj files with one mesh are supported for now), and which textures to
use. Textures are either loaded from files or existing offscreen images. This is
backed by the simple parser in **common::ObjFile**.
- **MultiMeshResource** is meant for complex models with multiple meshes.
Internally, meshes will be loaded with **common::ModelLoader**, which is backed
by [Assimp library](http://www.assimp.org). We will need to specify the path to
the model file, and the directory where textures are located, assuming all
textures are in the same directory.

In both cases, **Model** will hold the vertex buffer. If any textures are loaded
from existing offscreen images, the user will be responsible for keeping the
existence of the images. For simplicity, we will bind all textures of the same
type to the same binding point as an array. The user need to specify the binding
point for each type.

The user also need to specify shaders after the builder is constructed. By
default, shader modules are released to save the host memory after one model is
built. If multiple models will share shaders, the user can use
**ModelBuilder::AutoReleaseShaderPool** to prevent shaders from being auto
released, and thus to save memory I/O.

#### 3.3.4.2 Bind optional resources

If any of optional resources are used, the user is responsible for keeping the
existence of them, since **ModelBuilder** does not own them.

- Per-instance vertex buffers are used for instanced drawing. Apart from
providing the buffer itself, the user should also specify vertex input
attributes and the size of vertex data for each instance.
- Uniform buffers can be bound in two steps. The first step is to provide the
binding information, which declares that at which shader stages, which binding
points are used by uniform buffers, and the array length at each point. The
second step is to bind buffers. The user may bind multiple buffers to one
binding point.
- For push constants, since we can only have one push constant block in the
entire pipeline, there is no binding point or array length to specify. We will
need to specify at which shader stages they will be used, and which instances of
**PushConstant** will provide the data (targeting different offsets).

#### 3.3.4.3 Build and update model

After all the information above are set, the user can build a new instance of
**Model**. After that, all internal states of the **ModelBuilder** instance will
be invalidated, hence it should be simply discarded.

**Model** will preserve the instance of **PipelineBuilder** for future updates.
For example, if the window is resized, the render pass and framebuffer size may
change, in which case the user would need to inform **Model**. Internally, the
graphics pipeline will be rebuilt. Besides that, if the user wants to make an
opaque object transparent, or the other way around, **Model** should also be
updated. As a future optimization, we may make color blend a part of dynamic
states to avoid rebuilding the entire pipeline for changing transparency.

### 3.3.5 Text renderer (text and text_util)

#### 3.3.5.1 CharLoader and TextLoader

**common::CharLib** loads textures of each single character. If we want to use
those textures, here are some options we have:

1. Directly bind the texture of the character that we want to render, and render
characters one by one. In this way we need to do lots of bindings and render
calls, and the textures of characters might not be stored together on the
device, hence we might not be able to take advantage of the cache.
2. Put all textures in one array, and pass the index of the character that we
want to render via push constants or uniform buffers. However, we still need to
define the length of the array, but we may not know which characters will be
used when we write the shader code, especially if we want to render unicode.
Besides, this does not take advantage of the cache either.
3. Render the characters that we might want to use onto a character atlas image.
After this, the textures of single characters can be destroyed. When we want to
render any of these characters, we only need to bind that atlas texture.

**CharLoader** helps us take the last approach. It takes in all characters that
might be used later, loads their textures via **common::CharLib**, renders them
to a texture that is just big enough, and records the glyph of each character
and where to find that character on the atlas texture. For example, if we want
to display the frame rate with numbers, we can tell it that we might use numbers
from 0 to 9, and it will create such a texture:

![](https://docs.google.com/uc?id=17NVplvW-bEUrtdWdmXgsl-wkaOr4iW-Q)

The order of characters does not matter at this step, as long as we keep track
of the location of each of them. The color of characters appears red because
this texture only has R channel. Note that this texture has been flipped
vertically to make readers easier to understand it, but it is actually upside
down, which makes it easier to use later because of the coordinate system.

Sometimes we have some fixed texts. They might be a word, a sentence or
anything else as long as we don't change the order of characters. Take
displaying the frame rate as an example, we may always want to show the text
"FPS:" on the screen. We may change its color, transparency and location on the
screen, but the text itself does not change. In this case, we can use
**TextLoader** to render those characters onto a separate texture, so that later
when we want to display this text, we only need to bind one texture and do one
render call:

![](https://docs.google.com/uc?id=12TBpr-_zRjC23QE1pPPkGA41pw-fCWht)

This texture has also been flipped for readability. **TextLoader** first uses
**CharLoader** to render all characters onto one texture without a specific
order, and then render different texts to different textures. These two classes
actually share a lot in common: same shaders, same descriptors, same render
pass, etc.

In the future, we may support rendering texts with signed distance fields.

#### 3.3.5.2 StaticText and DynamicText

![](https://docs.google.com/uc?id=1MM_9CuUaxBL8wPPsdW0se9-XkvsxBXKc)

- **StaticText** is backed by **TextLoader**. The user should pass in a list of
static texts that will be rendered later to its constructor. For rendering, the
user should use the index of text in the list to specify which one to render.
- **DynamicText** is backed by **CharLoader**. The user should pass in a list of
texts, containing all the characters that might be rendered later. For
rendering, the user can pass in any string consisting of those characters.

The user can specify different RGBA and location for the text in each frame. For
now, we only support the horizontal layout and alignment. More support will be
added in the future. With these two classes, the user would not need to directly
use **CharLoader** and **TextLoader** for simple scenes.

Note that if the user wants to construct several **StaticText**s and/or
**DynamicText**s, using **ShaderModule::AutoReleaseShaderPool** may be of great
help to save memory I/O since they are expected to reuse shaders.

# 4. Applications (lighter/application/)

## 4.1 Triangle scene (triangle)

![](https://docs.google.com/uc?id=1FgCZ40kg9e0POyJjoB-GlJJLtxWe6y6K)

This is the most basic scene, where we don't have any mesh or texture, but one
blinking triangle. This proves all the basic functionality for onscreen
rendering (vertex buffer, push constant, swapchain, command buffer, graphics
pipeline and render pass) and alpha blending are working. We have a [breakdown of the code](https://github.com/lun0522/lighter/tree/master/lighter/application/vulkan)
to illustrate the usage of them. If all resources on the device are destroyed
properly, the context will be destructed at last, and we should see the log
"Context destructed properly" in the debug compilation mode.

## 4.2 Cube scene (cube)

![](https://docs.google.com/uc?id=1UipW6M6x5uLKSOJdTOPNus86CFaicl-i)

The cube model is loaded by our lightweight .obj file loader. The statue image
on the cube proves we can load images from files, and we properly associate it
with an image sampler and set up descriptors. The text showing the frame rate
proves the offscreen rendering pipeline is working. Since text rendering
requires loading the same shaders several times, if the reference counting and
auto release pool work well, in the debug compilation mode, we should see some
logs saying cache hits for shaders.

## 4.3 Nanosuit scene (nanosuit)

![](https://docs.google.com/uc?id=14GFRdlf1qYqZem45e5YKLbjrL5ni84LP)

The nanosuit model is loaded with [Assimp library](http://www.assimp.org). The
user can use keyboard to control the camera. This scene uses a uniform buffer.
It also tests the skybox loading and reference counting for textures, since the
skybox texture is also used to compute the reflection on the glasses of nanosuit
model.

## 4.4 Planet and asteroids scene (planet)

![](https://docs.google.com/uc?id=1fWg5UHeHI--hP21l_8Rf3ti97OM10Ydq)

This scene is mainly built for testing instanced drawing and the performance.

## 4.5 Troop scene (troop)

![](https://docs.google.com/uc?id=1RrNKchGwcwSoYg7FiosBCE8_HkzMMjTW)

This scene uses deferred shading. In the geometry pass, we have to store the
position, normal and albedo of each fragment to different images. Besides, we
are also using a depth stencil image. It proves that we just need to specify the
usage history of each image, and high-level extensions are able to lower it to a
render pass.

## 4.6 Post effect scene (post_effect)

![](https://docs.google.com/uc?id=1Bod00_1qByL7MT3XEa5yRdya_9wg5DQT)

In this scene, we are adding a simple effect to a texture image using a compute
shader. It proves the basic functionality of compute pipeline is working.

## 4.7 Aurora sketcher

This is re-implementing my previous project [AuroraSketcher](https://github.com/lun0522/AuroraSketcher).
The way of rendering aurora came from:

> Lawlor, Orion & Genetti, Jon. (2011). Interactive Volume Rendering Aurora on the
GPU. Journal of WSCG. 19. 25-32.

In the original paper, the aurora path was generated offline, and the camera was
not on the earth. In this application, we want to enable users to design the
aurora path by playing with splines, and view the generated aurora from any
point on the surface of the earth.

### 4.7.1 Editor scene (aurora/editor/)

![](https://docs.google.com/uc?id=1DY6eP0vVAMPrrEKBaj_cpkzrUS9Sw_fz)

In the editor scene, the user can interact with the 3D earth model:

- Zoom in/out, by scrolling up/down.
- Rotate the earth, by dragging any point of the earth. After the dragging ends,
the earth will keep rotating, and slow down gradually.
- Turn on/off daylight to make it easier to find a location, by clicking on the
**DAYLIGHT** button.

By clicking on the **EDITING** button, we can enter/exit the edit mode. In the
edit mode, the earth won't interact with the user (but the user can still zoom
in/out). Besides, four buttons will show on the top of the frame, where the
first three are used to select which aurora path to edit. Each path is
represented as a spline. The user may:

- Add a control point, by right clicking on any point on the earth where there
isn't a control point yet.
- Remove a control point, by right clicking on an existing control point.
- Move a control points, by dragging an existing control point.

After selecting the last button on the top, **VIEWPOINT**, the user can change
from which location do we view the generated aurora. Note that only one of these
four buttons can be selected at the same time.

After the design is done, the user can click on the **AURORA** button, and the
application will enter the viewer scene to show the generated aurora. We can
come back to the editor scene at any time, and the design won't be lost.

### 4.7.2 Viewer scene (aurora/viewer/)

![](https://docs.google.com/uc?id=1zQcfdJs-mSSy_fCu81MLTd6FRoDJY8n-)

In this scene, the user can move the cursor to change the direction of camera.
This scene appears not so complex, but under the scene these are happening:

1. Render aurora paths that can be seen from the user specified viewpoint to a
2D image as line strips.
2. Use a compute shader to make those paths wider, since we cannot make line
width greater than 1 on may hardware when we render them.
3. Use several compute shaders to generate the distance field using jump
flooding algorithm.
4. Render aurora paths using ray marching, accelerated by using the distance
field.

# 5. Acknowledgements

## 5.1 Tutorials

- [OpenGL tutorial by Joey de Vries](https://learnopengl.com)
- [Vulkan tutorial by Alexander Overvoorde](https://vulkan-tutorial.com)
- [Vulkan examples by Sascha Willems](https://github.com/SaschaWillems/Vulkan)

## 5.2 Dependencies

- [Abseil library](https://abseil.io)
- [Assimp library](http://www.assimp.org)
- [Bazel build system](https://bazel.build)
- [FreeType library](https://www.freetype.org)
- [GLAD library](https://github.com/Dav1dde/glad)
- [GLFW library](https://www.glfw.org)
- [GLM library](https://glm.g-truc.net)
- [Google Test library](https://github.com/google/googletest)
- [STB library](https://github.com/nothings/stb)
- [Vulkan SDK](https://vulkan.lunarg.com)

## 5.3 Resources

- The table of contents of each README is generated with [github-markdown-toc](https://github.com/ekalinin/github-markdown-toc).
- Class inheritance graphs are generated with [Doxygen](http://www.doxygen.nl)
and [Graphviz](http://www.graphviz.org).
- Fonts, frameworks, 3D models and textures are in a separate [resource repo](https://github.com/lun0522/resource).