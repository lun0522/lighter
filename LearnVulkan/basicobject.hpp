//
//  basicobject.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef basicobject_hpp
#define basicobject_hpp

#include <vulkan/vulkan.hpp>

class GLFWwindow;

namespace VulkanWrappers {
    using namespace std;
    
    class Application;
    
    class Instance {
        VkInstance instance;
        
    public:
        Instance() {}
        void init();
        void cleanup() { vkDestroyInstance(instance, nullptr); }
        ~Instance() { cleanup(); }
        
        VkInstance &operator*(void) { return instance; }
        const VkInstance &operator*(void) const { return instance; }
    };
    
    class Surface {
        const Application &app;
        VkSurfaceKHR surface;
        
    public:
        Surface(const Application &app) : app{app} {}
        void init();
        void cleanup();
        ~Surface() { cleanup(); }
        
        VkSurfaceKHR &operator*(void) { return surface; }
        const VkSurfaceKHR &operator*(void) const { return surface; }
    };
    
    struct Queues {
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        // queues are implicitly cleaned up with physical device
    };
    
    struct PhysicalDevice {
        Application &app;
        VkPhysicalDevice phyDevice;
        
    public:
        PhysicalDevice(Application &app) : app{app} {}
        PhysicalDevice(Application &app, const VkPhysicalDevice& vkPhyDevice)
        : app{app}, phyDevice{vkPhyDevice} {}
        void init();
        void cleanup() {} // implicitly cleaned up
        
        VkPhysicalDevice &operator*(void) { return phyDevice; }
        const VkPhysicalDevice &operator*(void) const { return phyDevice; }
    };
    
    struct Device {
        Application &app;
        VkDevice device;
        
    public:
        Device(Application &app) : app{app} {}
        void init();
        void cleanup() { vkDestroyDevice(device, nullptr); }
        ~Device() { cleanup(); }
        
        VkDevice &operator*(void) { return device; }
        const VkDevice &operator*(void) const { return device; }
    };
}

#endif /* basicobject_hpp */
