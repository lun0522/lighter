//
//  util.cc
//
//  Created by Pujun Lun on 2/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "util.h"

#include <sstream>
#include <unordered_map>

namespace util {
namespace {

using std::ifstream;
using std::string;

} /* namespace */

const string& ReadFile(const string& path) {
  static std::unordered_map<string, string> kLoadedText{};
  auto loaded = kLoadedText.find(path);
  if (loaded == kLoadedText.end()) {
    ifstream file(path);
    file.exceptions(ifstream::failbit | ifstream::badbit);
    if (!file.is_open())
      throw std::runtime_error{"Failed to open file: " + path};

    try {
      std::ostringstream stream;
      stream << file.rdbuf();
      string code = stream.str();
      loaded = kLoadedText.insert({path, code}).first;
    } catch (ifstream::failure e) {
      throw std::runtime_error{"Failed to read file: " + e.code().message()};
    }
  }
  return loaded->second;
}

} /* namespace util */
