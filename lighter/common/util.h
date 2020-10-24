//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_UTIL_H
#define LIGHTER_COMMON_UTIL_H

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "third_party/absl/base/optimization.h"
#include "third_party/absl/flags/parse.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"

#ifdef NDEBUG
#define LOG(stream) ::lighter::common::util::Logger{stream}          \
                        << ::lighter::common::util::PrintTime << ' '
#else  /* !NDEBUG */
#define LOG(stream) ::lighter::common::util::Logger{stream}          \
                        << '[' << ::lighter::common::util::PrintTime \
                        << absl::StrFormat(" %s:%d] ", __FILE__, __LINE__)
#endif /* NDEBUG */

#define LOG_INFO LOG(std::cout)
#define LOG_EMPTY_LINE LOG_INFO
#define LOG_ERROR LOG(std::cerr)

#ifdef NDEBUG
#define FATAL(error) throw std::runtime_error{error}
#else  /* !NDEBUG */
#define FATAL(error)                          \
  throw std::runtime_error{absl::StrFormat(   \
      "%s() in %s at line %d: %s",            \
      __func__, __FILE__, __LINE__, error)}
#endif /* NDEBUG */

#define ASSERT_TRUE(expr, error)              \
    if (!ABSL_PREDICT_TRUE(expr))             \
      FATAL(error)

#define ASSERT_FALSE(expr, error)             \
    ASSERT_TRUE(!(expr), error)

#define ASSERT_HAS_VALUE(object, error)       \
    ASSERT_TRUE(object.has_value(), error)

#define ASSERT_NO_VALUE(object, error)        \
    ASSERT_FALSE(object.has_value(), error)

#define ASSERT_NON_NULL(pointer, error)       \
    ASSERT_FALSE(pointer == nullptr, error)

#define ASSERT_NON_EMPTY(container, error)    \
    ASSERT_FALSE(container.empty(), error)

#define FATAL_IF_NULL(pointer)                \
    (ABSL_PREDICT_TRUE(pointer != nullptr)    \
        ? pointer                             \
        : FATAL(#pointer " is nullptr"))

namespace lighter {
namespace common {
namespace util {

// Parses command line arguments. This should be called in main().
inline void ParseCommandLine(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
}

// Prints the current time in "YYYY-MM-DD HH:MM:SS.fff" format.
std::ostream& PrintTime(std::ostream& os);

// This class simply wraps a std::ostream. When an instance of it gets
// destructed, it will append std::endl to the end;
class Logger {
 public:
  explicit Logger(std::ostream& os) : os_{os} {}

  // This class is neither copyable nor movable.
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  ~Logger() { os_ << std::endl; }

  template <typename Streamable>
  Logger& operator<<(const Streamable& streamable) {
    os_ << streamable;
    return *this;
  }

 private:
  std::ostream& os_;
};

// Returns the index of the first element that satisfies 'predicate'.
// If there is no such element, returns 'absl::nullopt'.
template <typename ContentType>
absl::optional<int> FindIndexOfFirst(
    absl::Span<const ContentType> container,
    const std::function<bool(const ContentType&)>& predicate) {
  const auto first_itr = std::find_if(container.begin(), container.end(),
                                      predicate);
  if (first_itr == container.end()) {
    return absl::nullopt;
  }
  return std::distance(container.begin(), first_itr);
}

// Moves 'element' to the specified 'index' of 'container'. 'container' will be
// resized if necessary.
template <typename ContentType>
void SetElementWithResizing(ContentType&& element, int index,
                            std::vector<ContentType>* container) {
  if (index >= container->size()) {
    container->resize(index + 1);
  }
  (*container)[index] = std::forward<ContentType>(element);
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

// Moves all elements of 'src' to the end of 'dst'.
template <typename ContentType>
void VectorAppend(std::vector<ContentType>& dst,
                  std::vector<ContentType>& src) {
  dst.reserve(src.size() + dst.size());
  std::move(src.begin(), src.end(), std::back_inserter(dst));
  src.clear();
}

// Erases elements in 'container' that satisfies the 'predicate'.
template <typename Container, typename Predicate>
void EraseIf(const Predicate& predicate, Container* container) {
  auto iter = container->begin();
  while (iter != container->end()) {
    if (predicate(*iter)) {
      container->erase(iter++);
    } else {
      ++iter;
    }
  }
}

// Returns the largest extent that does not exceed 'original_extent', and has
// the specified 'aspect_ratio'.
template <typename ExtentType>
ExtentType FindLargestExtent(const ExtentType& original_extent,
                             float aspect_ratio) {
  ExtentType effective_extent = original_extent;
  if (original_extent.x > original_extent.y * aspect_ratio) {
    effective_extent.x = original_extent.y * aspect_ratio;
  } else {
    effective_extent.y = original_extent.x / aspect_ratio;
  }
  return effective_extent;
}

// Returns whether
inline bool IsPowerOf2(unsigned int x) {
  if (x <= 0) {
    return false;
  }
  return (x & (x - 1)) == 0;
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

// Returns 'pointer', assuming 'ExpectedType' and 'ActualType' are the same.
template <typename ExpectedType, typename ActualType>
inline const ExpectedType* GetPointerIfTypeExpectedImpl(
    const ActualType* pointer, std::true_type) {
  return pointer;
}

// Returns nullptr, assuming 'ExpectedType' and 'ActualType' are different.
template <typename ExpectedType, typename ActualType>
inline const ExpectedType* GetPointerIfTypeExpectedImpl(
    const ActualType* pointer, std::false_type) {
  return nullptr;
}

} /* namespace internal */

// Returns a pointer of 'ExpectedType'. If elements of 'container' are of
// 'ExpectedType', the returned pointer will point to its underlying data.
// Otherwise, returns nullptr.
template <typename ExpectedType, typename ContentType>
const ExpectedType* GetPointerIfTypeExpected(
    const std::vector<ContentType>& container) {
  return internal::GetPointerIfTypeExpectedImpl<ExpectedType, ContentType>(
      container.data(), std::is_same<ExpectedType, ContentType>());
}

} /* namespace util */
} /* namespace common */
} /* namespace lighter */

#endif /* LIGHTER_COMMON_UTIL_H */
