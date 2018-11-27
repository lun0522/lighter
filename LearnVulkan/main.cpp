//
//  main.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

using namespace std;

int main(int argc, const char * argv[]) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Learn Vulkan", nullptr, nullptr);
    
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    cout << extensionCount << " extensions supported" << endl;
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
