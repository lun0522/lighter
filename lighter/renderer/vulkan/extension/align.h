//
//  align.h
//
//  Created by Pujun Lun on 10/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_ALIGN_H
#define LIGHTER_RENDERER_VULKAN_ALIGN_H

// Alignment requirements of Vulkan:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
#define ALIGN_SCALAR(type) alignas(sizeof(type))
#define ALIGN_VEC4 alignas(sizeof(float) * 4)
#define ALIGN_MAT4 alignas(sizeof(float) * 4)

#endif /* LIGHTER_RENDERER_VULKAN_ALIGN_H */
