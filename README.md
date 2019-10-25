Jessie Steamer
---

A low-level graphics engine.

Table of Contents
---

   * [0. Introduction](#0-introduction)
   * [1. Common modules (jessie_steamer/common/)](#1-common-modules-jessie_steamercommon)
      * [1.1 Camera (camera)](#11-camera-camera)
      * [1.2 Character library (char_lib)](#12-character-library-char_lib)
      * [1.3 File utils (file)](#13-file-utils-file)
      * [1.4 Model loader (model_loader)](#14-model-loader-model_loader)
      * [1.5 Reference counting (ref_count)](#15-reference-counting-ref_count)
      * [1.6 Time utils (time)](#16-time-utils-time)
      * [1.7 Window manager (window)](#17-window-manager-window)
   * [2. OpenGL wrappers (jessie_steamer/wrapper/opengl/)](#2-opengl-wrappers-jessie_steamerwrapperopengl)
   * [3. Vulkan wrappers (jessie_steamer/wrapper/vulkan/)](#3-vulkan-wrappers-jessie_steamerwrappervulkan)
      * [3.1 Contexts (basic_context, basic_object, validation and window_context)](#31-contexts-basic_context-basic_object-validation-and-window_context)
      * [3.2 Low-level wrappers](#32-low-level-wrappers)
         * [3.2.1 Buffer (buffer)](#321-buffer-buffer)
            * [3.2.1.1 UniformBuffer and PushConstant](#3211-uniformbuffer-and-pushconstant)
            * [3.2.1.2 StaticPerVertexBuffer and DynamicPerVertexBuffer](#3212-staticpervertexbuffer-and-dynamicpervertexbuffer)
            * [3.2.1.3 PerInstanceBuffer](#3213-perinstancebuffer)
            * [3.2.1.4 TextureBuffer, OffscreenBuffer, DepthStencilBuffer and MultisampleBuffer](#3214-texturebuffer-offscreenbuffer-depthstencilbuffer-and-multisamplebuffer)
         * [3.2.2 Command (command)](#322-command-command)
         * [3.2.3 Descriptor (descriptor)](#323-descriptor-descriptor)
         * [3.2.4 Image (image)](#324-image-image)
         * [3.2.5 Pipeline (pipeline and pipeline_util)](#325-pipeline-pipeline-and-pipeline_util)
         * [3.2.6 Render pass (render_pass and render_pass_util)](#326-render-pass-render_pass-and-render_pass_util)
         * [3.2.7 Swapchain (swapchain)](#327-swapchain-swapchain)
         * [3.2.8 Synchronization (synchronization)](#328-synchronization-synchronization)
      * [3.3 High-level wrappers](#33-high-level-wrappers)
         * [3.3.1 Model renderer (model)](#331-model-renderer-model)
            * [3.3.1.1 Load vertex data, textures and shaders](#3311-load-vertex-data-textures-and-shaders)
            * [3.3.1.2 Bind per-instance vertex buffers, uniform buffers and push constants](#3312-bind-per-instance-vertex-buffers-uniform-buffers-and-push-constants)
            * [3.3.1.3 Build and update the model](#3313-build-and-update-the-model)
         * [3.3.2 Text renderer (text and text_util)](#332-text-renderer-text-and-text_util)
   * [4. Applications (jessie_steamer/application/)](#4-applications-jessie_steamerapplication)
      * [4.1 Triangle scene (triangle)](#41-triangle-scene-triangle)
      * [4.2 Cube scene (cube)](#42-cube-scene-cube)
      * [4.3 Nanosuit scene (nanosuit)](#43-nanosuit-scene-nanosuit)
      * [4.4 Planet and asteroids scene (planet)](#44-planet-and-asteroids-scene-planet)
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

Before running any application, shaders should be compiled by executing
[compile_shaders.sh](https://github.com/lun0522/jessie-steamer/blob/master/compile_shaders.sh)
(no command line arguments needed). To run applications, since we use the
[Bazel build system](https://bazel.build), this is how we run from command line:

```bash
bazel run -c opt --copt=-DUSE_VULKAN //jessie_steamer/application/vulkan:triangle
```

The compilation mode is specified with the `-c` flag. See details on [Bazel website](https://docs.bazel.build/versions/master/command-line-reference.html#flag--compilation_mode).
We do turn on the verbose logging mode if not compiled with `-c opt`. You can
also just build it with Bazel and directly launch the executable, which is more
useful for debugging with external tools:

```bash
bazel build -c dbg --copt=-DUSE_VULKAN //jessie_steamer/application/vulkan:triangle
bazel-bin/jessie_steamer/application/vulkan/triangle --resource_folder=<path to resource folder> --shader_folder=<path to shader folder> --vulkan_folder=<path to Vulkan SDK folder>
```

Here is the meaning of each flag (note that these are not required if you 
directly use `bazel run`):

- *resource_folder*: Since resource files are stored in a separate
[resource repo](https://github.com/lun0522/resource), it must have been
downloaded, and the path to it should be specified with this flag.
- *shader_folder*: The local address of [this folder](https://github.com/lun0522/jessie-steamer/tree/master/jessie_steamer/shader)
should be specified with this flag.
- *vulkan_folder*: The [Vulkan SDK](https://vulkan.lunarg.com) must have been 
downloaded, and the path to the platform specific folder should be specified
with this flag. For MacOS, this would be
`vulkansdk-macos-<version number>/macOS`.

This README introduces the modules we created, and the decisions we made when we
design them. The usage of classes and util functions is put right before the
declaration in header files. You can start with a
[Vulkan "Hello Triangle" example](https://github.com/lun0522/jessie-steamer/tree/master/jessie_steamer/application/vulkan),
and then take a look at other applications under the jessie_steamer/application
folder, which are good examples of how to use other features such as rendering
models, skybox and text.

# 1. Common modules (jessie_steamer/common/)

These modules are shared by OpenGL and Vulkan wrappers. Most of the code is
independent of graphics APIs. For those exceptions, we have added preprocessors
to make sure if we are not compiling for a certain API, we won't need to link to
anything related to it.

## 1.1 Camera (camera)

![](https://docs.google.com/uc?id=1W05t3I4SXnW6lEGaR5nh-si7s4sy46Ji)

**Camera** models a prospective camera. It exposes methods for updating the
field of view, screen size, position and direction, and nothing more. It does
not care about where the control signal comes from. **UserControlledCamera** is
the one that we should use in applications. It can respond to inputs from
cursor, scroll and keyboard. These two classes are not merged into one because
in the future we may support more off-screen rendering features, in which case
the control signal may come from a log file or some other sources, not just
directly from the user.

## 1.2 Character library (char_lib)

**CharLib** is backed by [FreeType library](https://www.freetype.org). It simply
loads textures of individual characters. We have higher level wrappers to render
texts with characters loaded by it, so the user may not need to directly use it.

## 1.3 File utils (file)

These util classes are used to load raw data or images from files, and load
vertex data from Wavefront .obj files. The .obj file loader is pretty
lightweight, and it can only load a single mesh, which is useful enough for
simple 3D models like cubes and spheres.

## 1.4 Model loader (model_loader)

**ModelLoader** is backed by [Assimp library](http://www.assimp.org) and is used
to load complex 3D models. The vertex data and textures of each mesh will be
gathered in **ModelLoader::MeshData**. We have higher level wrappers of it to
render models loaded by it, so the user may not need to directly use it.

## 1.5 Reference counting (ref_count)

Smart pointers are good enough for managing resources on the host side in this
project, but we still need something else to manage resources on the device,
especially if they are loaded from files. For example, we might have loaded some
textures for rendering a skybox, and later we might want to use the same
textures to compute reflections when rendering other objects. To avoid loading
textures again in order to save disk I/O, one option is to have a shared pointer
to reference to the textures, and pass the pointer around. We think this is not
very convenient, and we may not know which textures we have already loaded,
especially when some textures are loaded by the model loader and we never type
in the file path by ourselves.

Another option is to put the pointer in a hashmap, and create an identifier of
it. We can use the file path as the identifier in this case, so that we won't
load a texture twice from the same path, as long as it is still in the hashmap.
We generalize this idea to create this reference counting class, so that we can
do the same thing for other resources, such as shader files. All reference
counted objects of the same class will be put in one hashmap, and will be erased
from it if the reference count drops to zero.

Shader files are a bit different, since after we construct a graphics pipeline,
we can already destroy them. Even if the next pipeline to build may use the same
shaders, their reference counts may have already dropped to zero before that.
Hence, we allow the user to set a policy to determine whether the object should
be destroyed when the reference count drops to zero. The user can change the
policy at anytime, and force cleaning out all objects with zero reference counts
at anytime. To make it even easier to use, we introduced the auto release pool,
which sets the policy automatically, to prevent any resource from being
destroyed before it goes out of scope. The user may create an instance of it to
preserve loaded shader files, then construct all models and pipelines. When the
instance goes out of scope, shader files will be released if unused anymore.
This is similar to using `std::lock_guard`.

## 1.6 Timer (timer)

**Timer** is mainly used to compute the frame rate, which is updated every
second. The user may also query how long has it been since the timer is
launched.

## 1.7 Window manager (window)

**Window** is backed by [GLFW library](https://www.glfw.org). The user can
register callbacks for cursor, scroll and keyboard inputs, and check whether
the window is resized or closed in the main loop. For Vulkan applications, it is
also responsible for providing the names of required extensions and help create
**VkSurfaceKHR**.

# 2. OpenGL wrappers (jessie_steamer/wrapper/opengl/)

Not implemented yet.

# 3. Vulkan wrappers (jessie_steamer/wrapper/vulkan/)

We don't aim to implement OpenGL APIs on the top of Vulkan APIs, but we will try
to create different levels of wrappers to enable the user to take advantage of
Vulkan to meet different needs. The user may simply use the high-level wrappers
to render simple models and texts, which should not be much harder than using
OpenGL APIs (hope so), or directly use low-level wrappers for flexibility.

## 3.1 Contexts (basic_context, basic_object, validation and window_context)

This project was first built following the
[Vulkan tutorial by Alexander Overvoorde](https://vulkan-tutorial.com). It is an
amazing tutorial, but it has also sacrificed something to be an amazing one. It
is very understandable that the author put almost all the code in one source
file, without encapsulating components into different classes, however, this
style is not good for building reusable infrastructures. We need to have
hierarchies, inheritances, interfaces and so on, hence we need to make different
design decisions.

The first decision we had to make is how to provide a context. Many Vulkan
functions need access to **VkInstance**, **VkDevice** and many other Vulkan
objects. A lot of them are shared when we render different targets, which makes
them the "context". The original design of this project was to have a wrapper
for each of these Vulkan objects, and use one monolithic context class to hold
instances of all wrappers, including **Instance**, **Command** and **Pipeline**,
etc. That made it easier to follow tutorials, but problems revealed as the
project got bigger:

1. It was convenient to pass a context to a function so that it can fetch
everything it needs, but this made it harder to know which part of the context
was actually used. For example, in the past when we [built a pipeline](https://github.com/lun0522/jessie-steamer/blob/1f307a02b25e5f7957b173b96f244ead6cbae53a/jessie_steamer/wrapper/vulkan/pipeline.cc#L47)
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

They are truly shared throughout the entire graphics program. Since these
members are not defined in the same file as the context, we still need to use
forward declarations, but we no longer need to do the same for other wrappers
like **Swapchain** and **Pipeline**. When wrappers need to interact with each
other, we would pass the original Vulkan objects as much as we can, so that
wrappers don't need to depend on each other. For onscreen rendering, we created
a window context, which holds a basic context and other members related to the
window:

- **Window**
- **Surface**
- **Swapchain**

We also created a Bazel target `//jessie_steamer/wrapper/vulkan:on_screen` which
exposes only the files needed for onscreen rendering, so applications only need
to depend on this target and hold one instance of **WindowContext**.

Another decision we made was to create and destroy Vulkan objects in a more RAII
way. The tutorial does not wrap Vulkan objects that much. Each of those objects
is created by a function and destroyed by another, where the destruction
functions are called in the reversed order of construction. However, we found
that some of these orders are not necessary. For example, render passes and
pipelines are independent, and they can be created/destroyed in arbitrary order.
Hence, we extracted objects that should be dealt with carefully, and put them in
**BasicContext** and **WindowContext**, where we rely on the declaration order
(which also determines destruction order in nature) to make sure they are
created/destroyed properly. Other wrapper objects that are not in the contexts
can be created/destroyed in arbitrary order, so the user need not worry about it
at all.

We need to pay extra attention to the swapcahin recreation, which happens if the
window is resized or moved to another monitor. Some objects will be affected,
such as **Swapchain** (since the size of swapchain images changes) and
**Pipeline** (since the viewport changes). One way to solve this is to follow
the tutorial and have methods like `Create()` and `Cleanup()` in those wrappers,
so that we can destroy expired resources and recreate them. However, the user of
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
just need to update a part of the information and rebuild the pipeline. Since
**Pipeline** is usually managed by higher level wrappers (**Model** and
**Text**), the user only need to call `Model::Update()` and `Text::Update()`.
- For now, the user need to manage **RenderPass** and **DepthStencilImage**
directly, hence the user need to update and rebuild **RenderPass** with
**RenderPassBuilder**, and create a new **DepthStencilImage** with the updated
window size. We might find a better way to wrap them.

**Surface** is a bit different. Since it is owned by **WindowContext** but
initialized by **BasicContext**, it has an `Init()` method that need to be
called after construction, and we need to use an extra boolean to track whether
it has been initialized. The good thing is, this is handled by
**WindowContext**, so the user would not need to worry about it.

## 3.2 Low-level wrappers

### 3.2.1 Buffer (buffer)

![](https://docs.google.com/uc?id=1uVO1xYuN5EX2tBtQ-Yxb44eFEsVMRW7g)

(**PushConstant** is not shown in the inheritance graph, since it does not own
an underlying buffer object.)

#### 3.2.1.1 UniformBuffer and PushConstant

Both of them are used to pass uniform data. When constructed, both of them will
allocate memory on both host and device. Before rendering a frame, the user
should acquire a pointer to the host data, populate it and flush to the device.

According to the [Vulkan specification](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap36.html#limits-minmax),
to be compatible of all devices, we only allow **PushConstant** to have at most
128 bytes for each frame. Besides, since it is possible to push constant to one
graphics pipeline using multiple instances of **PushConstant**, we also check
the total size in **Pipeline** to make sure it is still no more than 128 bytes.

#### 3.2.1.2 StaticPerVertexBuffer and DynamicPerVertexBuffer

**StaticPerVertexBuffer** is more commonly use, which stores vertex data that
does not change once constructed.

**DynamicPerVertexBuffer** provides a buffer to which we can send different data
of different size before each render call. The buffer tracks the current size of
itself. If we try to send data whose size is larger than the current buffer
size, it will create a new buffer internally. Otherwise, it reuses the old
buffer. For example, when we want to display the frame rate per-second, we need
to render one rectangle for each digit, but the digits and the number of digits
may always change, so we can take advantage of this buffer.

These two buffers are usually managed by high-level wrappers (*i.e.*
model renderer and text renderer), and the user may not need to directly
instantiate them.

#### 3.2.1.3 PerInstanceBuffer

This buffer simply copies one monolithic chunk of data from the host, and can be
bound to the specified binding point. The user may need to directly instantiate
this type of vertex buffer.

#### 3.2.1.4 TextureBuffer, OffscreenBuffer, DepthStencilBuffer and MultisampleBuffer

These buffers manage device image memory. Among them, **TextureBuffer** is the
only one that copies data from the host at construction, since it stores texture
images loaded from files. Other buffers are used as rendering targets.

These buffers are usually managed by the **Image** classes, and the user may not
need to directly instantiate them.

### 3.2.2 Command (command)

![](https://docs.google.com/uc?id=1UlOc-ts3a55EmIPSYheY_h2jsrw6lQrl)

- **OneTimeCommand** is used for commands that will be executed only once. For
example, we only need to transfer data to the static vertex buffer once. The
user can specify which device queue to use.
- **PerFrameCommand** is used for commands that will be executed every frame.
This is used for onscreen rendering, and it handles the synchronization
internally, so that the user won't need to use semaphores and fences directly.

Both of them are meant to be used directly by the user. We will add more command
classes for offscreen rendering and other use cases, and always shield the
complexity of synchronization from the user.

### 3.2.3 Descriptor (descriptor)

![](https://docs.google.com/uc?id=18xYBBhYkBQmSu_3TA8AeK8cW-pevtPXn)

- **StaticDescriptor** is meant for descriptors that are only updated once
during the command buffer recording. It is supported on all hardware.
- **DynamicDescriptor** is meant for descriptors that are updated multiple times
during the command buffer recording. It requires the extension
*VK_KHR_push_descriptor*, hence it may not be available on all hardware.

They are usually managed by high-level wrappers (*i.e.* model renderer and text
renderer). The user would still need to inform the model renderer of the
resources declared in the customized shaders. For high-level wrappers, we choose
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

### 3.2.4 Image (image)

![](https://docs.google.com/uc?id=1v3dHjXWTJbwLQKYgqk67zHxHcUAuxWqr)

**Image** is the base class of all other image classes. It provides accessors
to the image view, extent, format and sample count (which will be greater than 1
when multisampling is used). These information should be enough for setting an
image as an attachment in the render pass. All of its subclasses can be directly
used by the user, except for **SwapchainImage**, which is managed by
**Swapchain**:

- **SwapchainImage** wraps an image retrieved from the swapchain.
- **OffscreenImage** creates an image that can be used as offscreen rendering
target.
- **TextureImage** copies an image from the host memory to the device memory.
- **DepthStencilImage** creates an image that can be used as single-sample depth
stencil attachment.
- **MultisampleImage** exposes two convenient constructors to the user. One of
them creates a multisample image that can be used as color attachment, while
another one creates a multisample image for depth stencil.

If we want to do multisampling for color images, we would need to create a
**MultisampleImage**, and then resolve to either **OffscreenImage** or
**SwapchainImage**. If we want to do multisampling for depth stencil images, we
don't need to resolve the multisample image. To make life easier,
**MultisampleImage** provides another convenient constructor that takes in an
optional multisampling mode. If this mode is `absl::nullopt`, it will return a
single-sample image, otherwise, it will return a multisample image. The user
would use the same code to use the depth stencil image in either case.

![](https://docs.google.com/uc?id=1Q4wvHWjwxGHfFvaUvIhQgmZ-ykuehdZQ)

**SamplableImage** is the interface for images that are associated with
samplers. We may want to sample from either **OffscreenImage** or
**TextureImage**, hence we created two classes to wrap them and implement the
interface:

- **SharedTexture** takes in the source path of a single image or a cubemap, and
stores a reference to the corresponding image resource on the device. To avoid
loading the same image from the disk twice, the resource on the device is
reference counted.
- **UnownedOffscreenTexture** takes in a const pointer to an offscreen texture.
It does not own the texture, hence the user is responsible for keeping the
existence of the texture.

### 3.2.5 Pipeline (pipeline and pipeline_util)

We provide a builder class to create instances of **Pipeline**. Through
**PipelineBuilder** the user may set up:

- depth and stencil testing
- front face direction
- multisampling
- vertex input bindings and attributes
- descriptor set layouts
- push constant ranges
- viewport and scissor
- render pass and subpass
- color blending
- shaders used in the pipeline

We also provide helper functions in *pipeline_util* for setting the viewport,
color blending and vertex input bindings and attributes.

If any state is changed, the user can reuse the builder, update the states and
rebuild the pipeline. For example, after the window is resized, the user may
need to create a new pipeline with updated render pass and viewport. Since
shaders are referenced counted, it is up to the user to decide whether or not
to release shaders after the pipeline is built.

Setting up such a pipeline can be tiring. This is usually managed by high-level
wrappers (*i.e.* model renderer and text renderer), hence the user would not
directly interact with it.

### 3.2.6 Render pass (render_pass and render_pass_util)

We provide a builder class to create instances of **RenderPass**. Through
**RenderPassBuilder**, the user may set:

- number of framebuffers
- attachments and associated images
- multisampling relationships
- subpasses and dependencies between them

When used to build **RenderPass**, the internal states of the builder would not
be changed, hence later the user may update the builder and create another
**RenderPass** with similar settings, which is useful, for example, when the
window is resized and we need to recreate the render pass with swapchain images
of the updated size.

Since setting up a render pass requires a bit of understanding of Vulkan
concepts, such as the image layout, pipeline stage and memory access mask, and
most of them are very similar when rendering simple scenes, we aim to provide
utils to create render pass builders with commonly used settings in
*render_pass_util*. For now, we only have a naive one for rendering simple
objects and text. More of them will be added for more complex scenes and
rendering techniques, such as deferred rendering.

### 3.2.7 Swapchain (swapchain)

**Swapchain** holds wrappers of images retrieved from the swapchain. If
constructed with multisampling enabled, it will also hold a multisample color
image. This class is managed by **WindowContext**, hence the user would not need
to directly interact with it. See more details in how we designed the
[window context](https://github.com/lun0522/jessie-steamer#31-contexts-basic_context-basic_object-validation-and-window_context).

### 3.2.8 Synchronization (synchronization)

**Semaphores** are used for GPU->GPU sync, while **Fences** are used for
GPU->CPU sync. **PerFrameCommand** uses them to coordinate command buffer
recordings and submissions. The user would not need to directly use them for
rendering simple scenes.

## 3.3 High-level wrappers

### 3.3.1 Model renderer (model)

**Model** is built to shield a great deal of low-level details from the user, so
that a few lines of code should be enough to display a model on the screen. We
will illustrate how to use **ModelBuilder** and **Model**.

#### 3.3.1.1 Load vertex data, textures and shaders

The user need to specify how to load the model vertex data and textures when
constructing **ModelBuilder**.

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

Shaders are added after a builder is constructed. By default, shader modules are
released to save the host memory. However, this will cause more I/O the next
time we rebuild the model. If multiple models share the same shaders, the user
can use **ModelBuilder::AutoReleaseShaderPool** (which is an alias of
**PipelineBuilder::AutoReleaseShaderPool**) to prevent shaders from being auto
released. Especially when rendering texts, the same shaders are expected to be
used for multiple times, and the auto release pool will be of great help.

#### 3.3.1.2 Bind per-instance vertex buffers, uniform buffers and push constants

All these information are optional. If any resources are used, the user is
responsible for keeping the existence of those resources, since **ModelBuilder**
does not own them.

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

#### 3.3.1.3 Build and update the model

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

### 3.3.2 Text renderer (text and text_util)

#### 3.3.2.1 CharLoader and TextLoader

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
3. Render the characters that we might want to use onto a big texture. After
this, the textures of single characters can be destroyed. When we want to render
any of these characters, we only need to bind that big texture.

**CharLoader** helps us take the last approach. It takes in all characters that
might be used later, loads their textures via **common::CharLib**, renders them
to a texture that is just big enough, and records the glyph of each character
and where to find that character on the big texture. For example, if we want to
display the frame rate with numbers, we can tell it that we might use numbers
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
**CharLoader** to render all characters onto one big texture without a specific
order, and then render different texts to different textures. These two classes
actually share a lot in common: same shaders, same descriptors, same render
pass, etc.

#### 3.3.2.2 StaticText and DynamicText

![](https://docs.google.com/uc?id=10MEvDtUZsmKL8oskpESNn4ENzs7F1MXZ)

**StaticText** is backed by **TextLoader**. The user should pass in a list of
static texts that will be rendered later to its constructor. **DynamicText**
is backed by **CharLoader**. The user should pass in a list of texts that
contain all the characters that might be rendered later. When rendering the
text, for **StaticText**, the user should use the index of the text in the list
to specify which one to render. For **DynamicText**, the user can pass in any
string, as long as the string does not contain any character that
**DynamicText** does not expect.

The user can specify different RGBA and location for the text in each frame. For
now, we only support the horizontal layout and alignment. More support will be
added in the future. With these two classes, the user would not need to directly
use **CharLoader** and **TextLoader** for simple scenes.

# 4. Applications (jessie_steamer/application/)

## 4.1 Triangle scene (triangle)

![](https://docs.google.com/uc?id=1FgCZ40kg9e0POyJjoB-GlJJLtxWe6y6K)

This is the most basic scene, where we don't have any mesh or texture, but one
blinking triangle. This proves all the basic functionality for onscreen
rendering (vertex buffer, command buffer, swapchain, render pass and graphics
pipeline) is working. We have a [breakdown of the code](https://github.com/lun0522/jessie-steamer/tree/master/jessie_steamer/application/vulkan)
to illustrate the usage of them. If all resources on the device are destroyed
properly, the context will be destructed at last, and we should see the log
"Context destructed properly" in the debug compilation mode.

## 4.2 Cube scene (cube)

![](https://docs.google.com/uc?id=1UipW6M6x5uLKSOJdTOPNus86CFaicl-i)

The cube model is loaded by our lightweight .obj file loader. The statue image
on the cube proves we can load images from files. The text that shows the frame
rate proves the off-screen pipeline and alpha blending are working. Since text
rendering requires loading the same shaders several times, if the reference
counting and auto release pool work well, in the debug compilation mode, we
should see some logs saying cache hits for shaders.

## 4.3 Nanosuit scene (nanosuit)

![](https://docs.google.com/uc?id=14GFRdlf1qYqZem45e5YKLbjrL5ni84LP)

The nanosuit model is loaded with [Assimp library](http://www.assimp.org). The
user can use keyboard to control the camera. This scene also tests the skybox
loading and reference counting for textures, since the skybox texture is also
used to compute the reflection on the glasses of nanosuit model.

## 4.4 Planet and asteroids scene (planet)

![](https://docs.google.com/uc?id=1fWg5UHeHI--hP21l_8Rf3ti97OM10Ydq)

This scene is mainly built for testing instanced drawing and the performance.

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
- [GLFW library](https://www.glfw.org)
- [GLM library](https://glm.g-truc.net)
- [STB library](https://github.com/nothings/stb)
- [Vulkan SDK](https://vulkan.lunarg.com)

## 5.3 Resources

- The table of contents of each README is generated with [github-markdown-toc](https://github.com/ekalinin/github-markdown-toc)
- Class inheritance graphs are generated with [Doxygen](http://www.doxygen.nl)
and [Graphviz](http://www.graphviz.org)
- Fonts, frameworks, 3D models and textures in a separate [resource repo](https://github.com/lun0522/resource)

---

2018.08 - 2019.06 You were here.