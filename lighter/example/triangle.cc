//
//  triangle.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/util.h"

namespace lighter {
namespace example {

class TriangleExample {
 public:
  explicit TriangleExample() {}

  // This class is neither copyable nor movable.
  TriangleExample(const TriangleExample&) = delete;
  TriangleExample& operator=(const TriangleExample&) = delete;

  void MainLoop() {}
};

} /* namespace example */
} /* namespace lighter */

int main(int argc, char* argv[]) {
  using namespace lighter::example;
  return ExampleMain<TriangleExample>(argc, argv);
}
