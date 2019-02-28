//
//  util.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "util.h"

#include <sstream>
#include <unordered_map>

using namespace std;

namespace util {

const string& ReadFile(const string& path) {
    static unordered_map<string, string> kLoadedText{};
    auto loaded = kLoadedText.find(path);
    if (loaded == kLoadedText.end()) {
        ifstream file(path);
        file.exceptions(ifstream::failbit | ifstream::badbit);
        if (!file.is_open())
            throw runtime_error{"Failed to open file: " + path};
        
        try {
            ostringstream stream;
            stream << file.rdbuf();
            string code = stream.str();
            loaded = kLoadedText.insert({path, code}).first;
        } catch (ifstream::failure e) {
            throw runtime_error{
                "Failed to read file: " + e.code().message()};
        }
    }
    return loaded->second;
}

} /* namespace util */
