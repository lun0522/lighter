//
//  ref_count.h
//
//  Created by Pujun Lun on 5/11/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_REF_COUNT_H
#define JESSIE_STEAMER_COMMON_REF_COUNT_H

#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"

namespace jessie_steamer {
namespace common {

template <typename ObjectType>
class RefCountedObject {
 public:
  template <typename... Args>
  static RefCountedObject Get(const std::string& identifier, Args&&... args) {
    auto found = ref_counter_.find(identifier);
    if (found == ref_counter_.end()) {
      auto inserted = ref_counter_.emplace(
          identifier,
          std::make_pair(absl::make_unique<ObjectType>(
              std::forward<Args>(args)...), 0));
      found = inserted.first;
    }
    auto& counter = found->second;
    ++counter.second;
    return RefCountedObject{identifier, counter.first.get()};
  }

  RefCountedObject(std::string identifier, const ObjectType* object_ptr)
      : identifier_{std::move(identifier)}, object_ptr_{object_ptr} {}

  // This class is only movable
  RefCountedObject(RefCountedObject&& rhs) noexcept {
    identifier_ = std::move(rhs.identifier_);
    object_ptr_ = rhs.object_ptr_;
    rhs.identifier_.clear();
  }

  RefCountedObject& operator=(RefCountedObject&& rhs) noexcept {
    std::swap(identifier_, rhs.identifier_);
    std::swap(object_ptr_, rhs.object_ptr_);
    return *this;
  }

  ~RefCountedObject() {
    if (identifier_.empty()) {
      return;
    }
    auto found = ref_counter_.find(identifier_);
    if (--found->second.second == 0) {
      ref_counter_.erase(found);
    }
  }

  const ObjectType* operator->() const { return object_ptr_; }

 private:
  using ObjectAndCount = std::pair<std::unique_ptr<ObjectType>, int>;
  static absl::flat_hash_map<std::string, ObjectAndCount> ref_counter_;
  std::string identifier_;
  const ObjectType* object_ptr_;
};

template <typename ObjectType>
absl::flat_hash_map<std::string,
                    typename RefCountedObject<ObjectType>::ObjectAndCount>
    RefCountedObject<ObjectType>::ref_counter_{};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_REF_COUNT_H */
