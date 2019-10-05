//
//  ref_count.h
//
//  Created by Pujun Lun on 5/11/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_REF_COUNT_H
#define JESSIE_STEAMER_COMMON_REF_COUNT_H

#include <memory>
#include <string>
#include <utility>

#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace common {

// Reference counted objects that use strings as the identifier. We can use the
// object with operators '.' and '->', as if using std smart pointers.
template <typename ObjectType>
class RefCountedObject {
 public:
  // The user should always call this to get an object. If any object with same
  // identifier is still living in the objects pool, it will be returned, and
  // its reference count will be increased. Otherwise, 'args' will be used to
  // construct a new object.
  template <typename... Args>
  static RefCountedObject Get(const std::string& identifier, Args&&... args) {
    auto found = ref_count_map_.find(identifier);
    if (found == ref_count_map_.end()) {
      auto inserted = ref_count_map_.emplace(
          identifier,
          std::make_pair(absl::make_unique<ObjectType>(
              std::forward<Args>(args)...), 0));
      found = inserted.first;
    }
    auto& counter = found->second;
    ++counter.second;
    return RefCountedObject{identifier, counter.first.get()};
  }

  // This class is only movable.
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
    auto found = ref_count_map_.find(identifier_);
    if (--found->second.second == 0) {
      ref_count_map_.erase(found);
    }
  }

  // Overloads.
  const ObjectType* operator->() const { return object_ptr_; }
  const ObjectType& operator*() const { return *object_ptr_; }

 private:
  RefCountedObject(std::string identifier, const ObjectType* object_ptr)
      : identifier_{std::move(identifier)}, object_ptr_{object_ptr} {}

  // An objects pool shared by all objects of the same class, where the key is
  // the identifier, and the value is a pair of the actual object and its
  // reference count. The object will be erased from the pool if it no longer
  // has any holder.
  using ObjectAndCount = std::pair</*object_ptr*/std::unique_ptr<ObjectType>,
                                   /*ref_count*/int>;
  static absl::flat_hash_map<std::string, ObjectAndCount> ref_count_map_;

  // Identifier of the object.
  std::string identifier_;

  // Pointer to the actual object.
  const ObjectType* object_ptr_;
};

template <typename ObjectType>
absl::flat_hash_map<std::string,
                    typename RefCountedObject<ObjectType>::ObjectAndCount>
    RefCountedObject<ObjectType>::ref_count_map_{};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_REF_COUNT_H */
