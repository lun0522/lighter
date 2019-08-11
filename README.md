Files that are waiting to be cleaned up:

common
- camera ✅
- char_lib ✅
- file ✅
- model_loader ✅
- ref_count ✅
- time ✅
- util ✅
- window ✅

wrapper
- basic_context ✅
- basic_object ✅
- buffer
- command
- descriptor
- image
- model
- pipeline
- render_pass
- swapchain
- synchronize ✅
- text
- text_util
- util
- validation ✅
- vertex_input_util
- window_context

application
- util
- cube
- nanosuit
- planet

# 0. Introduction

This project aims to build a low-level graphics engine with **reusable**
infrastructures, with either Vulkan or OpenGL as backend. The code follows
[Google C++ style guide](https://google.github.io/styleguide/cppguide.html).

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

## 3.1 Basics

### 3.1.1 Basic objects (basic_object)

### 3.1.2 Validation layer (validation)

### 3.1.3 Basic context (basic_context)

## 3.2 Buffer (buffer)

### 3.2.1 Data buffer

### 3.2.2 Image buffer

## 3.3 Command (command)

## 3.4 Descriptor (descriptor)

## 3.5 Image (image)

## 3.6 Model (model)

## 3.7 Pipeline (pipeline)

## 3.8 Render pass (render_pass)

## 3.9 Swapchain (swapchain)

## 3.10 Synchronization (synchronization)

## 3.11 Text renderer

### 3.11.1 Utils (text_util)

### 3.11.2 Renderer (text)

## 3.12 Window context (window_context)

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

Fonts, frameworks, 3D models and textures in my
[resource repo](https://github.com/lun0522/resource).