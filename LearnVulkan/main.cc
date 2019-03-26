//
//  main.cc
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>

#include "application/nanosuit.h"

int main(int argc, const char* argv[]) {
#ifdef DEBUG
  application::vulkan::NanosuitApp app{};
  app.MainLoop();
#else
  try {
    application::vulkan::NanosuitApp app{};
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif /* DEBUG */
  return EXIT_SUCCESS;
}
