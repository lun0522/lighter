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
- window_context

application
- util
- cube
- nanosuit
- planet

# 0. Introduction

This project aims to build a low-level graphics engine with **reusable**
infrastructures, with either Vulkan or OpenGL as backend. It should be able to
run on MacOS, and won't be difficult to run on Linux (just need to find suitable
third-party library binaries). The code follows [Google C++ style guide](https://google.github.io/styleguide/cppguide.html).

We use the [Bazel build system](https://bazel.build), so the applications are
run in this way from command line:

`bazel build -c opt //jessie_steamer/application/vulkan:cube`

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

## 3.1 Basic wrappers

The original design of this project was to use one monolithic context class to
hold instances of all other wrapper classes, including Instance, CommandBuffer
and Pipeline, etc. That made it easier to follow tutorials, but then problems
revealed as the project got bigger:

1. It was convenient to pass a context to a function so that it can fetch
everything it needs, but this made it harder to know which part of the context
was actually used. For example, in the past when we [built a pipeline](https://github.com/lun0522/jessie-steamer/blob/1f307a02b25e5f7957b173b96f244ead6cbae53a/jessie_steamer/wrapper/vulkan/pipeline.cc#L47)
we passed in the entire context but only touched Device, VkAllocationCallbacks
and Swapchain. Among them, Swapchain was actually used only once. It makes more
sense to only keep those frequently used members (like Instance and Device) in
the context, and pass others (like Swapchain) as arguments, so that we can more
easily know what does the function do internally.
2. It was very easy to create circular dependencies. Each wrapper class needs
the context to initialize itself and perform other operations, and the context
needs to hold instances of wrappers. This could be solved by using forward
declarations and pointers, but things got worse when wrappers classes need to
depend on each other. It also made it ugly to write the BUILD file for Bazel,
since the circular dependency is prohibited in BUILD files and thus we had to
to put all source files in one target. That means, all of them become visible
to the user, however, we actually don't want users to use some classes directly.
3. It made it harder to extend the program to render more complicated scenes.
For example, it was fine to put one Swapchain and one Pipeline in the context
if we were only going to render one cube, however, if we want to further render
some text on the frame, the Swapchain should be shared, but we would need more
Pipeline. Besides, when we prepare the texture for the text, we only need to
make one render call, but when we want to display it on the screen, we need to
make a render call every frame, hence we need different CommandBuffer in
different classes.
4. Some members are not necessary for off-screen rendering, such as
Surface, Swapchain and common::Window. Besides, we should not depend on GLFW in
the code and BUILD file.

The final choice is to create a basic context with minimal number of members:

- VkAllocationCallbacks
- Instance
- PhysicalDevice
- Device
- Queue
- DebugCallback (only in debug mode)

They are always required when we build a graphics program. Since they reside in
a different file (basic_object), we still need to use forward declarations, but
we no longer need to do the same for other wrappers like Swapchain and Pipeline.
When wrappers need to interact with each other, we would pass the original
Vulkan objects as much as we can, so that wrappers don't need to depend on each
other. For on-screen rendering, we created a window context, which holds a basic
context and other members related to the window:

- common::Window
- Surface
- Swapchain

We also created a Bazel target `//jessie_steamer/wrapper/vulkan:on_screen` which
exposed only the files needed for on-screen rendering, so applications only need
to depend on this target and hold one instance of WindowContext.

## 3.2 Low-level wrappers

### 3.2.1 Buffer (buffer)

### 3.2.2 Command (command)

### 3.2.3 Descriptor (descriptor)

### 3.2.4 Image (image)

### 3.2.5 Pipeline (pipeline)

### 3.2.6 Render pass (render_pass)

### 3.2.7 Swapchain (swapchain)

### 3.2.8 Synchronization (synchronization)

### 3.2.9 Window context (window_context)

## 3.3 High-level wrappers

### 3.3.1 Model renderer (model)

### 3.3.2 Text renderer (text and text_util)

# 4. Applications

## 4.1 Cube scene (cube)

## 4.2 Nanosuit scene (nanosuit)

## 4.3 Planet and asteroids scene (planet)

# 5. Acknowledgements

## 5.1 Tutorials

- [Learn OpenGL](https://learnopengl.com)
- [Vulkan tutorial](https://vulkan-tutorial.com)
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