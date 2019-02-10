//
//  vkobjects.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef vkobjects_hpp
#define vkobjects_hpp

namespace VulkanWrappers {
    class Instance {
        VkInstance instance;
    public:
        VkInstance &operator*(void) { return instance; }
        const VkInstance &operator*(void) const { return instance; }
        ~Instance() { vkDestroyInstance(instance, nullptr); }
    };
    
    class Surface {
        VkInstance &instance;
        VkSurfaceKHR surface;
    public:
        Surface(VkInstance &instance) : instance{instance} {}
        VkSurfaceKHR &operator*(void) { return surface; }
        const VkSurfaceKHR &operator*(void) const { return surface; }
        ~Surface() { vkDestroySurfaceKHR(instance, surface, nullptr); }
    };
    
    struct Device {
        VkDevice device;
    public:
        VkDevice &operator*(void) { return device; }
        const VkDevice &operator*(void) const { return device; }
        ~Device() { vkDestroyDevice(device, nullptr); }
    };
    
    struct PhysicalDevice {
        VkPhysicalDevice phyDevice;
    public:
        PhysicalDevice() {}
        PhysicalDevice(const VkPhysicalDevice& phyDevice) : phyDevice{phyDevice} {}
        VkPhysicalDevice &operator*(void) { return phyDevice; }
        const VkPhysicalDevice &operator*(void) const { return phyDevice; }
    };
}

#endif /* vkobjects_hpp */
