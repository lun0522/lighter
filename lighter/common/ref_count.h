//
//  ref_count.h
//
//  Created by Pujun Lun on 5/11/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_REF_COUNT_H
#define LIGHTER_COMMON_REF_COUNT_H

#include <memory>
#include <string>
#include <utility>

#include "lighter/common/util.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter::common {

// Each reference counted object uses a string as its identifier. We can use the
// object with operators '.' and '->', as if using std smart pointers.
// By default, an object will be destroyed if its reference count drops to zero.
// The user can use AutoReleasePool to change this behavior. See details in
// class comments.
template <typename ObjectType>
class RefCountedObject {
 public:
  // An instance of this class preserves reference counted objects of ObjectType
  // within its scope, even if the reference count of an object drops to zero.
  // When all pools associated with ObjectType go out of scope, objects with
  // zero reference count will be automatically released. The usage of it is
  // very similar to std::lock_guard.
  class AutoReleasePool {
   public:
    explicit AutoReleasePool() {
      RefCountedObject<ObjectType>::RegisterAutoReleasePool();
    }

    // This class is neither copyable nor movable.
    AutoReleasePool(const AutoReleasePool&) = delete;
    AutoReleasePool& operator=(const AutoReleasePool&) = delete;

    ~AutoReleasePool() {
      RefCountedObject<ObjectType>::UnregisterAutoReleasePool();
    }

    // Force the user to allocate on stack, in order to avoid overcomplications.
    void* operator new(size_t) = delete;
    void* operator new[](std::size_t) = delete;
  };

  // The user should always call this to get an object. If any object with same
  // identifier is still living in the objects pool, it will be returned, and
  // its reference count will be increased. Otherwise, 'args' will be used to
  // construct a new object.
  template <typename... Args>
  static RefCountedObject Get(const std::string& identifier, Args&&... args) {
    auto iter = ref_count_map().find(identifier);
    if (iter == ref_count_map().end()) {
      const auto inserted = ref_count_map().insert({
          identifier, typename ObjectPool::ObjectWithCounter{
              .object = std::make_unique<ObjectType>(std::forward<Args>(args)...),
              .ref_count = 0,
          }});
      iter = inserted.first;
    }
#ifndef NDEBUG
    else {
      LOG_INFO << "Cache hit: " << identifier;
    }
#endif  // !NDEBUG
    auto& ref_counted_object = iter->second;
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

  // If reference count drops to zero, and no auto release pool is active, the
  // object will be destructed.
  ~RefCountedObject() {
    if (identifier_.empty()) {
      return;
    }
    const auto iter = ref_count_map().find(identifier_);
    if (--iter->second.ref_count == 0 &&
        object_pool_.num_active_auto_release_pools == 0) {
      ref_count_map().erase(iter);
    }
  }

  // Overloads.
  const ObjectType* operator->() const { return object_ptr_; }
  const ObjectType& operator*() const { return *object_ptr_; }

  // Accessors.
  static bool has_active_auto_release_pool() {
    return object_pool_.num_active_auto_release_pools != 0;
  }

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
    int num_active_auto_release_pools = 0;
  };

  RefCountedObject(std::string identifier, const ObjectType* object_ptr)
      : identifier_{std::move(identifier)}, object_ptr_{object_ptr} {}

  // Increments the counter value of auto release pools.
  static void RegisterAutoReleasePool() {
    ++object_pool_.num_active_auto_release_pools;
  };

  // Reduces the counter value of auto release pools. If the counter value
  // drops to zero, releases objects with zero reference count.
  static void UnregisterAutoReleasePool() {
    if (--object_pool_.num_active_auto_release_pools == 0) {
      using ObjectWithCounter = typename ObjectPool::ObjectWithCounter;
      static const auto remove_unused =
          [](const std::pair<const std::string, ObjectWithCounter>& pair) {
            return pair.second.ref_count == 0;
          };
      common::util::EraseIf(remove_unused, ref_count_map());
    }
  };

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

}  // namespace lighter::common

#endif // LIGHTER_COMMON_REF_COUNT_H
