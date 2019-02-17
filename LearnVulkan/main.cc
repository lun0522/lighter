//
//  main.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>

#include "application.h"

using namespace std;

int main(int argc, const char *argv[]) {
    try {
        vulkan::Application app{"triangle.vert.spv", "triangle.frag.spv"};
        app.MainLoop();
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
