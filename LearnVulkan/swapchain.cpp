//
//  swapchain.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "swapchain.hpp"

#include <algorithm>
#include <iostream>

#include "utils.hpp"

namespace VulkanWrappers {
    const vector<const char*> SwapChain::requiredExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    
    bool SwapChain::hasSwapChainSupport(const VkSurfaceKHR &surface,
                                        const VkPhysicalDevice &device) {
        try {
            cout << "Checking extension support required for swap chain..." << endl << endl;
            
            vector<string> required{requiredExtensions.begin(), requiredExtensions.end()};
            auto extensions {Utils::queryAttribute<VkExtensionProperties>
                ([&device](uint32_t *count, VkExtensionProperties *properties) {
                    return vkEnumerateDeviceExtensionProperties(device, nullptr, count, properties);
                })
            };
            auto getName = [](const VkExtensionProperties &property) -> const char* {
                return property.extensionName;
            };
            Utils::checkSupport<VkExtensionProperties>(required, extensions, getName);
        } catch (const exception &e) {
            return false;
        }
        
        // physical device may support swap chain but maybe not compatible with window system
        // so we need to query details
        uint32_t formatCount, modeCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, nullptr);
        return formatCount != 0 && modeCount != 0;
    }
    
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR> &available) {
        // if surface has no preferred format, we can choose any format
        if (available.size() == 1 && available[0].format == VK_FORMAT_UNDEFINED)
            return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        
        for (const auto& candidate : available) {
            if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
                candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return candidate;
        }
        
        // if our preferred format is not supported, simply choose the first one
        return available[0];
    }
    
    VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR> &available) {
        // FIFO mode is guaranteed to be available, but not properly supported by some drivers
        // we will prefer IMMEDIATE mode over it
        VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
        
        for (const auto& candidate : available) {
            if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) {
                return candidate;
            } else if (candidate == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                bestMode = candidate;
            }
        }
        
        return bestMode;
    }
    
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                VkExtent2D currentExtent) {
        
        if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            currentExtent.width = max(capabilities.minImageExtent.width, currentExtent.width);
            currentExtent.width = min(capabilities.maxImageExtent.width, currentExtent.width);
            currentExtent.height = max(capabilities.minImageExtent.height, currentExtent.height);
            currentExtent.height = min(capabilities.maxImageExtent.height, currentExtent.height);
            return currentExtent;
        }
    }
    
    SwapChain::SwapChain(const VkSurfaceKHR &surface,
                         const VkDevice &device,
                         const VkPhysicalDevice &physicalDevice,
                         VkExtent2D currentExtent,
                         const unordered_set<uint32_t> &queueFamilies)
    : device{device} {
        // surface capabilities
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
        VkExtent2D extent = chooseSwapExtent(surfaceCapabilities, currentExtent);
        
        // surface formats
        auto surfaceFormats{Utils::queryAttribute<VkSurfaceFormatKHR>
            ([&surface, &physicalDevice](uint32_t *count, VkSurfaceFormatKHR *formats) {
                return vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, count, formats);
            })
        };
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
        
        // present modes
        auto presentModes{Utils::queryAttribute<VkPresentModeKHR>
            ([&surface, &physicalDevice](uint32_t *count, VkPresentModeKHR *modes) {
                return vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, count, modes);
            })
        };
        VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
        
        // how many images we want to have in swap chain
        uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount > 0) // can be 0 if no maximum
            imageCount = min(surfaceCapabilities.maxImageCount, imageCount);
        
        VkSwapchainCreateInfoKHR swapChainInfo{};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainInfo.surface = surface;
        swapChainInfo.minImageCount = imageCount;
        swapChainInfo.imageFormat = surfaceFormat.format;
        swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapChainInfo.imageExtent = extent;
        swapChainInfo.imageArrayLayers = 1;
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // can be different for post-processing
        swapChainInfo.preTransform = surfaceCapabilities.currentTransform; // may apply transformations
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // may change alpha channel
        swapChainInfo.presentMode = presentMode;
        swapChainInfo.clipped = VK_TRUE; // don't care about color of pixels obscured
        swapChainInfo.oldSwapchain = VK_NULL_HANDLE;
        
        if (queueFamilies.size() == 1) {
            // if only one queue family will access this swap chain
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        } else {
            // specify which queue families will share access to images
            // we will draw on images in swap chain from graphics queue
            // and submit on presentation queue
            vector<uint32_t> indices{queueFamilies.begin(), queueFamilies.end()};
            swapChainInfo.queueFamilyIndexCount = static_cast<uint32_t>(indices.size());
            swapChainInfo.pQueueFamilyIndices = indices.data();
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        }
        
        if (vkCreateSwapchainKHR(device, &swapChainInfo, nullptr, &swapChain) != VK_SUCCESS)
            throw runtime_error{"Failed to create swap chain"};
        
        imageFormat = surfaceFormat.format;
        imageExtent = extent;
        createImages();
    }
    
    void SwapChain::createImages() {
        // image count might be different since previously we only set a minimum
        uint32_t imageCount;
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        images.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());
        
        // use image view to specify how will we use these images
        // (color, depth, stencil, etc)
        imageViews.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            VkImageViewCreateInfo imageViewInfo{};
            imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewInfo.image = images[i];
            imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 1D, 2D, 3D, cube maps
            imageViewInfo.format = imageFormat;
            // `components` enables swizzling color channels around
            imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // `subresourceRange` specifies image's purpose and which part should be accessed
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewInfo.subresourceRange.baseMipLevel = 0;
            imageViewInfo.subresourceRange.levelCount = 1;
            imageViewInfo.subresourceRange.baseArrayLayer = 0;
            imageViewInfo.subresourceRange.layerCount = 1;
            
            if (vkCreateImageView(device, &imageViewInfo, nullptr, &imageViews[i]) != VK_SUCCESS)
                throw runtime_error{"Failed to create image view"};
        }
    }
    
    SwapChain::~SwapChain() {
        for (const auto& imageView : imageViews)
            vkDestroyImageView(device, imageView, nullptr);
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
}
