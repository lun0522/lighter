//
//  extension_test.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/ui/text_util.h"

#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <vector>

namespace lighter::ui::text {
namespace {

TEST(TextUtilTest, GenerateQuadIndicesBuildsSequentialRects) {
  const std::vector<uint32_t> indices = GenerateQuadIndices(/*num_rects=*/2);
  const std::vector<uint32_t> expected{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
  EXPECT_EQ(indices, expected);
}

TEST(TextUtilTest, AppendCharPosAndTexCoordNormalizesToNdc) {
  std::vector<base::Vertex2D> vertices;
  AppendCharPosAndTexCoord(/*pos_bottom_left=*/{0.0f, 0.5f},
                           /*pos_increment=*/{0.5f, 0.5f},
                           /*tex_coord_bottom_left=*/{0.0f, 0.0f},
                           /*tex_coord_increment=*/{1.0f, 1.0f}, &vertices);
  ASSERT_EQ(vertices.size(), 4u);
  EXPECT_EQ(vertices[0].pos, (glm::vec2{-1.0f, 0.0f}));
  EXPECT_EQ(vertices[1].pos, (glm::vec2{0.0f, 0.0f}));
  EXPECT_EQ(vertices[2].pos, (glm::vec2{0.0f, 1.0f}));
  EXPECT_EQ(vertices[3].pos, (glm::vec2{-1.0f, 1.0f}));
  EXPECT_EQ(vertices[0].tex_coord, (glm::vec2{0.0f, 0.0f}));
  EXPECT_EQ(vertices[2].tex_coord, (glm::vec2{1.0f, 1.0f}));
}

}  // namespace
}  // namespace lighter::ui::text
