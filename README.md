Files that are waiting to be cleaned up:

wrapper
- buffer
- command
- descriptor
- image
- model
- pipeline
- render_pass
- swapchain
- text
- text_util
- util
- vertex_input_util

application
- util
- cube
- nanosuit
- planet

# 0. Introduction

This project aims to build a low-level graphics engine with **reusable**
infrastructures, with either OpenGL or Vulkan as backend. It should be able to
run on MacOS, and won't be difficult to run on Linux (just need to find suitable
third-party library binaries). The code follows [Google C++ style guide](https://google.github.io/styleguide/cppguide.html)
and only uses the features of C++11 (enhanced by [Abseil library](https://abseil.io)).

Before running any application, shaders should be compiled by executing
[compile_shaders.sh](https://github.com/lun0522/jessie-steamer/blob/master/compile_shaders.sh)
(no command line arguments needed). To run applications, since we use the [Bazel build system](https://bazel.build),
this is how we build and run from command line:

`bazel build -c opt --copt=-DUSE_VULKAN //jessie_steamer/application/vulkan:cube`

# 1. Common modules (common/)

## 1.1 Camera (camera)

## 1.2 Character library (char_lib)

## 1.3 File utils (file)

## 1.4 Model loader (model_loader)

## 1.5 Reference counting (ref_count)

## 1.6 Time utils (time)

## 1.7 Window manager (window)

# 2. OpenGL wrappers (wrapper/opengl/)

# 3. Vulkan wrappers (wrapper/vulkan/)

## 3.1 Contexts (basic_context, basic_object and window_context)

This project was first built following the [Vulkan tutorial by Alexander Overvoorde](https://vulkan-tutorial.com).
It is an amazing tutorial, but it has also sacrificed something to be an amazing
one. It is very understandable that the author put almost all the code in one
source file, without encapsulating components into different classes, however,
this style is not good for building reusable infrastructures. We need to have
hierarchies, inheritances, interfaces and so on, hence we need to make different
design decisions.

The first decision we had to make is how to provide a context. Many Vulkan
functions need access to VkInstance, VkDevice and many other Vulkan objects.
A lot of them are shared when we render different targets, which makes them the
"context". The original design of this project was to have a wrapper for each of
these Vulkan objects, and use one monolithic context class to hold instances of
all wrappers, including **Instance**, **CommandBuffer** and **Pipeline**, etc.
That made it easier to follow tutorials, but problems revealed as the project
got bigger:

1. It was convenient to pass a context to a function so that it can fetch
everything it needs, but this made it harder to know which part of the context
was actually used. For example, in the past when we [built a pipeline](https://github.com/lun0522/jessie-steamer/blob/1f307a02b25e5f7957b173b96f244ead6cbae53a/jessie_steamer/wrapper/vulkan/pipeline.cc#L47)
we passed in the entire context but only touched **Device**,
**HostMemoryAllocator** and **Swapchain**. Among them, **Swapchain** was
actually used only once. It makes more sense to only keep those frequently used
members (like **Instance** and **Device**) in the context, and pass others (like
**Swapchain**) as arguments, so that we can more easily know what does the
function do internally.
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
we need to make a render call every frame, hence we need different
**CommandBuffer** in different classes.
4. Some members are not necessary for off-screen rendering, such as **Surface**,
**Swapchain** and **Window**. Besides, we should not depend on GLFW in the code
and BUILD file. When we do on-screen rendering, if the window is resized, we
need to recreate the resources that are affected by the frame size, such as
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
wrappers don't need to depend on each other. For on-screen rendering, we created
a window context, which holds a basic context and other members related to the
window:

- **Window**
- **Surface**
- **Swapchain**

We also created a Bazel target `//jessie_steamer/wrapper/vulkan:on_screen` which
exposes only the files needed for on-screen rendering, so applications only need
to depend on this target and hold one instance of **WindowContext**.

Another decision we made was to create and destroy Vulkan objects in a more RAII
way. The tutorial does not wrap Vulkan objects that much. Each of those objects
is created by a function and destroyed by another, where the destruction
functions are called in the reversed order of construction. However, we found
that some of these orders are not necessary. For example, render pass and
pipeline are independent, and they can be created/destroyed in arbitrary order.
Hence, we extracted objects that should be dealt with carefully in RAII
and put them in **BasicContext** and **WindowContext**, where we rely on the
declaration order (which also determines destruction order in nature) to make
sure they are created/destroyed properly. Other wrapper objects that are not in
the contexts can be created/destroyed in arbitrary order, so the user need not
worry about it at all.

We need to pay extra attention to the swapcahin recreation, which happens if the
window is resized or moved to another monitor. Some objects will be affected,
such as **Swapchain** (since swapchain image size changes) and **Pipeline**
(since viewport changes). One way to solve this is to follow the tutorial and
have methods like `Create()` and `Cleanup()` in those wrappers, so that we can
destroy expired resources and recreate them. However, the user of them would
need to worry about when and in what order to call these methods. If an
exception is thrown between calling `Cleanup()` and `Create()`, or if the user
calls `Cleanup()` and forgets to call `Create()`, the object will be left in an
uninitialized state, and the destructor of it would cause more trouble since it
tries to destroy uninitialized Vulkan objects, unless we make it more
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

## 3.2 Low-level wrappers

### 3.2.1 Buffer (buffer)

- **StaticPerVertexBuffer**
- **DynamicPerVertexBuffer**
- **PerInstanceBuffer**
- **UniformBuffer**
- **TextureBuffer**
- **OffscreenBuffer**
- **DepthStencilBuffer**
- **MultisampleBuffer**
- **PushConstant**

### 3.2.2 Command (command)

### 3.2.3 Descriptor (descriptor)

### 3.2.4 Image (image)

### 3.2.5 Pipeline (pipeline)

### 3.2.6 Render pass (render_pass)

### 3.2.7 Swapchain (swapchain)

### 3.2.8 Synchronization (synchronization)

## 3.3 High-level wrappers

### 3.3.1 Model renderer (model)

### 3.3.2 Text renderer (text and text_util)

# 4. Applications

## 4.1 Cube scene (cube)

## 4.2 Nanosuit scene (nanosuit)

## 4.3 Planet and asteroids scene (planet)

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

Fonts, frameworks, 3D models and textures in a separate [resource repo](https://github.com/lun0522/resource).