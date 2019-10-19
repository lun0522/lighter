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

#include "jessie_steamer/common/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace common {

// Each reference counted object uses a string as its identifier. We can use the
// object with operators '.' and '->', as if using std smart pointers.
// By default, an object will be destroyed if its reference count drops to zero.
// The user may call SetPolicy() to change the policy, in which case objects
// with zero reference counts will stay in the pool, until the policy changes
// again or the user calls Clean().
template <typename ObjectType>
class RefCountedObject {
 public:
  // The user should always call this to get an object. If any object with same
  // identifier is still living in the objects pool, it will be returned, and
  // its reference count will be increased. Otherwise, 'args' will be used to
  // construct a new object.
  template <typename... Args>
  static RefCountedObject Get(const std::string& identifier, Args&&... args) {
    auto found = ref_count_map().find(identifier);
    if (found == ref_count_map().end()) {
      const auto inserted = ref_count_map().emplace(
          identifier, typename ObjectPool::ObjectWithCounter{
              absl::make_unique<ObjectType>(std::forward<Args>(args)...),
              /*ref_count=*/0,
          });
      found = inserted.first;
    }
#ifndef NDEBUG
    else {
      LOG << "Cache hit: " << identifier << std::endl;
    }
#endif  /* !NDEBUG */
    auto& ref_counted_object = found->second;
    ++ref_counted_object.ref_count;
    return RefCountedObject{identifier, ref_counted_object.object.get()};
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
    const auto found = ref_count_map().find(identifier_);
    if (--found->second.ref_count == 0 && object_pool_.destroy_if_unused) {
      ref_count_map().erase(found);
    }
  }

  // If true, an object will be destroyed if its reference count drops to zero.
  static void SetPolicy(bool destroy_if_unused) {
    object_pool_.destroy_if_unused = destroy_if_unused;
    if (destroy_if_unused) {
      Clean();
    }
  }

  // Destroys all objects with zero reference counts in the pool;
  static void Clean() {
    using ObjectWithCounter = typename ObjectPool::ObjectWithCounter;
    static const auto remove_unused =
        [](const std::pair<const std::string, ObjectWithCounter>& pair) {
          return pair.second.ref_count == 0;
        };
    common::util::EraseIf(remove_unused, &ref_count_map());
  }

  // Overloads.
  const ObjectType* operator->() const { return object_ptr_; }
  const ObjectType& operator*() const { return *object_ptr_; }

 private:
  // An object pool shared by all objects of the same class. The key of
  // 'ref_count_map' is the identifier, and the value is the actual object and
  // its reference count.
  struct ObjectPool {
    struct ObjectWithCounter {
      std::unique_ptr<ObjectType> object;
      int ref_count;
    };
    using RefCountMap = absl::flat_hash_map<std::string, ObjectWithCounter>;

    RefCountMap ref_count_map;
    bool destroy_if_unused = true;
  };

  RefCountedObject(std::string identifier, const ObjectType* object_ptr)
      : identifier_{std::move(identifier)}, object_ptr_{object_ptr} {}

  // Accessors.
  static typename ObjectPool::RefCountMap& ref_count_map() {
    return object_pool_.ref_count_map;
  }

  // All objects of the same class will share one pool.
  static ObjectPool object_pool_;

  // Identifier of the object.
  std::string identifier_;

  // Pointer to the actual object.
  const ObjectType* object_ptr_;
};

template <typename ObjectType>
typename RefCountedObject<ObjectType>::ObjectPool
    RefCountedObject<ObjectType>::object_pool_{};

// An instance of this class preserves reference counted objects of ObjectType
// within its scope, even if the reference count of an object drops to zero.
// When it goes out of scope, objects with zero reference count will be
// automatically released. The usage of it is very similar to std::lock_guard.
template <typename ObjectType>
struct AutoReleasePool {
  AutoReleasePool() {
    RefCountedObject<ObjectType>::SetPolicy(/*destroy_if_unused=*/false);
  }

  ~AutoReleasePool() {
    RefCountedObject<ObjectType>::SetPolicy(/*destroy_if_unused=*/true);
  }
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_REF_COUNT_H */
