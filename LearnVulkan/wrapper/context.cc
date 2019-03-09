//
//  context.cc
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "context.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace vulkan {
namespace wrapper {
namespace {

void FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
  auto context = reinterpret_cast<Context*>(glfwGetWindowUserPointer(window));
  context->resized() = true;
}

} /* namespace */

void Context::Init(const std::string& name, uint32_t width, uint32_t height) {
  InitWindow(name, width, height);
  InitVulkan();
}

void Context::InitWindow(
    const std::string& name, uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this); // may retrive |this| in callback
  glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
}

void Context::InitVulkan() {
  if (is_first_time_) {
    instance_.Init();
#ifdef DEBUG
    // relay debug messages back to application
    callback_.Init(ptr(),
                   MessageSeverity::kWarning
                       | MessageSeverity::kError,
                   MessageType::kGeneral
                       | MessageType::kValidation
                       | MessageType::kPerformance);
#endif /* DEBUG */
    surface_.Init(ptr());
    physical_device_.Init(ptr());
    device_.Init(ptr());
    is_first_time_ = false;
  }
  swapchain_.Init(ptr());
  render_pass_.Init(ptr());
}

VkExtent2D Context::screen_size() const {
  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);
  return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

void Context::Recreate() {
  // do nothing if window is minimized
  int width = 0, height = 0;
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(*device_);
  Cleanup();
  InitVulkan();
}

void Context::Cleanup() {
  render_pass_.Cleanup();
  swapchain_.Cleanup();
}

bool Context::ShouldQuit() const {
  glfwPollEvents();
  return glfwWindowShouldClose(window_);
}

Context::~Context() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

} /* namespace wrapper */
} /* namespace vulkan */
