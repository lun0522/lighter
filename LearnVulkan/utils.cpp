//
//  utils.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "utils.hpp"

namespace Utils {
    vector<char> readFile(const string& filename) {
        // ios::ate means start reading from end of file
        // so that we know how large buffer do we need
        ifstream file{filename, ios::ate | ios::binary};
        size_t size{static_cast<size_t>(file.tellg())};
        vector<char> buffer(size);
        file.seekg(0);
        file.read(buffer.data(), size);
        file.close();
        return buffer;
    }
}
