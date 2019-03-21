//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef PUBLIC_UTIL_H
#define PUBLIC_UTIL_H

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#define NULL_FLAG 0
#define ASSERT_NONNULL(object, error) \
  if (object == nullptr)    throw std::runtime_error{error}
#define ASSERT_SUCCESS(event, error) \
  if (event != VK_SUCCESS)  throw std::runtime_error{error}
#define CONTAINER_SIZE(container) \
  static_cast<uint32_t>(container.size())

namespace util {

struct VertexAttrib {
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 tex_coord;

  VertexAttrib(const glm::vec3& pos,
               const glm::vec3& norm,
               const glm::vec2& tex_coord)
      : pos{pos}, norm{norm}, tex_coord{tex_coord} {}
};

template <typename AttribType>
std::vector<AttribType> QueryAttribute(
    const std::function<void(uint32_t*, AttribType*)>& enumerate) {
  uint32_t count;
  enumerate(&count, nullptr);
  std::vector<AttribType> attribs{count};
  enumerate(&count, attribs.data());
  return attribs;
}

template <typename AttribType>
void CheckSupport(
    const std::vector<std::string>& required,
    const std::vector<AttribType>& attribs,
    const std::function<const char*(const AttribType&)>& get_name) {
  std::unordered_set<std::string> available{attribs.size()};
  for (const auto& atr : attribs) {
    available.insert(get_name(atr));
  }

  std::cout << "Available:" << std::endl;
  for (const auto& avl : available) {
    std::cout << "\t" << avl << std::endl;
  }
  std::cout << std::endl;

  std::cout << "Required:" << std::endl;
  for (const auto& req : required) {
    std::cout << "\t" << req << std::endl;
  }
  std::cout << std::endl;

  for (const auto& req : required) {
    if (available.find(req) == available.end()) {
      throw std::runtime_error{"Requirement not satisfied: " + req};
    }
  }
}

extern const size_t kInvalidIndex;
template <typename ContentType>
size_t FindFirst(const std::vector<ContentType>& container,
                 const std::function<bool(const ContentType&)>& predicate) {
  auto first_itr = find_if(container.begin(), container.end(), predicate);
  return first_itr == container.end() ?
      kInvalidIndex : std::distance(container.begin(), first_itr);
}

const std::string& ReadFile(const std::string& path);

void LoadObjFile(const std::string& path,
                 int index_base,
                 std::vector<VertexAttrib>& vertices,
                 std::vector<uint32_t>& indices);

} /* namespace util */

#endif /* PUBLIC_UTIL_H */
