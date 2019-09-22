Files that are waiting to be cleaned up:

wrapper
- command
- model
- pipeline
- render_pass
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

This README introduces the modules we created, and the decisions we made when we
design the structure. The usage of each class is put right before the class
definition in the header file. Applications under the jessie_steamer/application
folder are good examples of how to use these classes to render simple scenes.

# 1. Common modules (jessie_steamer/common/)

## 1.1 Camera (camera)

## 1.2 Character library (char_lib)

## 1.3 File utils (file)

## 1.4 Model loader (model_loader)

## 1.5 Reference counting (ref_count)

## 1.6 Time utils (time)

## 1.7 Window manager (window)

# 2. OpenGL wrappers (jessie_steamer/wrapper/opengl/)

# 3. Vulkan wrappers (jessie_steamer/wrapper/vulkan/)

## 3.1 Contexts (basic_context, basic_object, validation and window_context)

This project was first built following the [Vulkan tutorial by Alexander Overvoorde](https://vulkan-tutorial.com).
It is an amazing tutorial, but it has also sacrificed something to be an amazing
one. It is very understandable that the author put almost all the code in one
source file, without encapsulating components into different classes, however,
this style is not good for building reusable infrastructures. We need to have
hierarchies, inheritances, interfaces and so on, hence we need to make different
design decisions.

The first decision we had to make is how to provide a context. Many Vulkan
functions need access to **VkInstance**, **VkDevice** and many other Vulkan
objects. A lot of them are shared when we render different targets, which makes
them the "context". The original design of this project was to have a wrapper
for each of these Vulkan objects, and use one monolithic context class to hold
instances of all wrappers, including **Instance**, **CommandBuffer** and
**Pipeline**, etc. That made it easier to follow tutorials, but problems
revealed as the project got bigger:

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
4. Some members are not necessary for offscreen rendering, such as **Surface**,
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

![](https://docs.google.com/uc?id=1uVO1xYuN5EX2tBtQ-Yxb44eFEsVMRW7g)

(**PushConstant** is not shown in the inheritance graph, since it does not own
an underlying buffer object.)

#### 3.2.1.1 **UniformBuffer** and **PushConstant**

Both of them are used to pass uniform data. When constructed, both of them will
allocate memory on both host and device. Before rendering a frame, the user
should acquire a pointer to the host data, populate it and flush to the device.

According to [Vulkan specification](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap36.html#limits-minmax),
to be compatible of all devices, we only allow **PushConstant** to have at most
128 bytes for each frame. Besides, since it is possible to push constant to one
graphics pipeline using multiple instances of **PushConstant**, we also check
the total size in **Pipeline** to make sure it is still no more than 128 bytes.

#### 3.2.1.2 **StaticPerVertexBuffer** and **DynamicPerVertexBuffer**

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

#### 3.2.1.3 **PerInstanceBuffer**

This buffer simply copies one monolithic chunk of data from the host, and can be
bound to the specified binding point. The user may need to directly instantiate
this type of vertex buffer.

#### 3.2.1.4 **TextureBuffer**, **OffscreenBuffer**, **DepthStencilBuffer** and **MultisampleBuffer**

These buffers manage device memory of images. Among them, **TextureBuffer** is
the only one that copies data from the host at construction, since it stores
texture images loaded from files. Other buffers are used as rendering targets.

These buffers are usually managed by the **Image** classes, and the user may not
need to directly instantiate them.

### 3.2.2 Command (command)

![](https://docs.google.com/uc?id=1UlOc-ts3a55EmIPSYheY_h2jsrw6lQrl)

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
which kind of **Descriptor** to use based the lifetime and update frequency of
the descriptor. For example:

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
to the image view, extent, format and sample count. These information should be
enough for setting an image as an attachment in the render pass. All of its
subclasses can be directly used by the user, except for **SwapchainImage**,
which is handled by **Swapchain**:

- **SwapchainImage** wraps an image retrieved from the swapchain.
- **OffscreenImage** creates an image that can be used as offscreen rendering
target.
- **TextureImage** copies a texture image from the host memory to the device
memory.
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
would use the same code in either case, except for specifying the multisampling
mode.

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

### 3.2.5 Pipeline (pipeline)

### 3.2.6 Render pass (render_pass)

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

### 3.3.2 Text renderer (text and text_util)

# 4. Applications (jessie_steamer/application/)

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

Class inheritance graphs are generated with [Doxygen](http://www.doxygen.nl)
and [Graphviz](http://www.graphviz.org).

Fonts, frameworks, 3D models and textures in a separate [resource repo](https://github.com/lun0522/resource).

---

2018.08 - 2019.06 You were here.