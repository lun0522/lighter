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

#include <string>
#include <vector>

namespace Utils {
    void checkExtensionSupport(const std::vector<std::string>& requiredExtensions);
    void checkValidationLayerSupport(const std::vector<std::string>& requiredLayers);
}

#endif /* UTILS_HPP */
#endif /* DEBUG */
