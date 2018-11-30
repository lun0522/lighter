//
//  utils.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG
#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>

namespace Utils {
    extern const std::vector<const char*> validationLayers;
    void checkVulkanSupport();
    void checkExtensionSupport();
    void checkValidationLayerSupport();
}

#endif /* UTILS_HPP */
#endif /* DEBUG */
