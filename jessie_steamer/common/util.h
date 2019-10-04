//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_UTIL_H
#define JESSIE_STEAMER_COMMON_UTIL_H

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "third_party/absl/base/optimization.h"
#include "third_party/absl/flags/parse.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/optional.h"

#ifdef NDEBUG
#define FATAL(error) throw std::runtime_error{error};
#else  /* !NDEBUG */
#define FATAL(error)                          \
  throw std::runtime_error{absl::StrFormat(   \
      "%s() in %s at line %d: %s",            \
      __func__, __FILE__, __LINE__, error)};
#endif /* NDEBUG */

#define ASSERT_TRUE(expr, error)              \
  if (!ABSL_PREDICT_TRUE(expr))               \
    FATAL(error);

#define ASSERT_FALSE(expr, error)             \
  if (ABSL_PREDICT_FALSE(expr))               \
    FATAL(error);

#define ASSERT_HAS_VALUE(object, error)       \
  if (!ABSL_PREDICT_TRUE(object.has_value())) \
    FATAL(error);

#define ASSERT_NO_VALUE(object, error)        \
  if (ABSL_PREDICT_FALSE(object.has_value())) \
    FATAL(error);

#define ASSERT_NON_NULL(pointer, error)       \
  if (!ABSL_PREDICT_TRUE(pointer != nullptr)) \
    FATAL(error);

#define ASSERT_NON_EMPTY(container, error)    \
  if (ABSL_PREDICT_FALSE(container.empty()))  \
    FATAL(error);

namespace jessie_steamer {
namespace common {
namespace util {

// Parses command line arguments. This should be called in main().
inline void ParseCommandLine(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
}

// Returns the index of the first element that satisfies 'predicate'.
// If there is no such element, returns 'absl::nullopt'.
template <typename ContentType>
absl::optional<int> FindIndexOfFirst(
    const std::vector<ContentType>& container,
    const std::function<bool(const ContentType&)>& predicate) {
  const auto first_itr = find_if(container.begin(), container.end(), predicate);
  return first_itr == container.end()
      ? absl::nullopt
      : absl::make_optional<int>(std::distance(container.begin(), first_itr));
}

// Moves 'element' to the specified 'index' of 'container'. 'container' will be
// resized if necessary.
template <typename ContentType>
void SetElementWithResizing(ContentType&& element, int index,
                            std::vector<ContentType>* container) {
  if (index >= container->size()) {
    container->resize(index + 1);
  }
  (*container)[index] = std::move(element);
}

// Removes duplicated elements from 'container' in-place, hence the size of
// 'container' may change if there exists any duplicate.
template <typename ContentType>
void RemoveDuplicate(std::vector<ContentType>* container) {
  if (container->size() > 1) {
    std::sort(container->begin(), container->end());
    const auto new_end = std::unique(container->begin(), container->end());
    container->resize(std::distance(container->begin(), new_end));
  }
}

// Helper class to enable using an enum class as the key of a hash table:
//   absl::flat_hash_map<KeyType, ValueType, EnumClassHash>;
struct EnumClassHash {
  template <typename EnumClass>
  std::size_t operator()(EnumClass value) const {
    return static_cast<std::size_t>(value);
  }
};

namespace internal {

// Returns a pointer to the underlying data of 'container', assuming
// 'ContentType' and 'ExpectedType' are the same.
template <typename ContentType, typename ExpectedType>
inline const ExpectedType* GetPointerIfTypeExpectedImpl(
    const std::vector<ContentType>& container, std::true_type) {
  return container.data();
}

// Returns nullptr, assuming 'ContentType' and 'ExpectedType' are different.
template <typename ContentType, typename ExpectedType>
inline const ExpectedType* GetPointerIfTypeExpectedImpl(
    const std::vector<ContentType>& container, std::false_type) {
  return nullptr;
}

} /* namespace internal */

// Returns a pointer of 'ExpectedType'. If elements of 'container' are of
// 'ExpectedType', the returned pointer will point to its underlying data.
// Otherwise, nullptr will be returned.
template <typename ContentType, typename ExpectedType>
const ExpectedType* GetPointerIfTypeExpected(
    const std::vector<ContentType>& container) {
  return internal::GetPointerIfTypeExpectedImpl<ContentType, ExpectedType>(
      container, std::is_same<ContentType, ExpectedType>());
}

} /* namespace util */
} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_UTIL_H */
