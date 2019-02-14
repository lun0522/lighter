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
    
    /** VkInstance is used to establish connection with Vulkan library and
     *      maintain per-application states.
     *
     *  Initialization:
     *      VkApplicationInfo (App/Engine/API name and version)
     *      Extensions to enable (required by GLFW and debugging)
     *      Layers to enable (required by validation layers)
     */
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
    
    /** VkSurfaceKHR interfaces with platform-specific window systems. It is
     *      backed by the window created by GLFW, which hides platform-specific
     *      details. It is not needed for off-screen rendering.
     *
     *  Initialization (by GLFW):
     *      VkInstance
     *      GLFWwindow
     */
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
    
    /** VkPhysicalDevice is a handle to a physical graphics card. We iterate
     *      through graphics devices to find one that supports swap chains.
     *      Then, we iterate through its queue families to find one family
     *      supporting graphics, and another one supporting presentation
     *      (possibly them are identical). All queues in one family share the
     *      same property, so we only need to find out the index of the family.
     *
     *  Initialization:
     *      VkInstance
     *      VkSurfaceKHR (since we need presentation support)
     */
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
    
    /** VkDevice interfaces with the physical device. We have to tell Vulkan
     *      how many queues we want to use. Noticed that the graphics queue and
     *      the present queue might be the same queue, we use a set to remove
     *      duplicated queue family indices.
     *
     *  Initialization:
     *      VkPhysicalDevice
     *      Physical device features to enable
     *      List of VkDeviceQueueCreateInfo (queue family index and how many
     *          queues do we want from this family)
     *      Extensions to enable (required by swap chains)
     *      Layers to enable (required by validation layers)
     */
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
    
    /** VkQueue is the queue associated with the logical device. When we create
     *      it, we can specify both queue family index and queue index (within
     *      that family).
     */
    struct Queues {
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        // queues are implicitly cleaned up with physical device
    };
}

#endif /* basicobject_hpp */
