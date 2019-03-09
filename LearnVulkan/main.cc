//
//  main.cc
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>

#include "application/triangle.h"

int main(int argc, const char* argv[]) {
  try {
    vulkan::application::TriangleApplication app;
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
