//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_UTIL_H
#define LIGHTER_COMMON_UTIL_H

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "third_party/absl/base/optimization.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/functional/function_ref.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"

#ifdef NDEBUG
#define LOG(stream)                         \
    ::lighter::common::util::Logger{stream} \
        << ::absl::StreamFormat(            \
               "%s ", ::lighter::common::util::GetCurrentTime())
#else  // !NDEBUG
#define LOG(stream)                         \
    ::lighter::common::util::Logger{stream} \
        << ::absl::StreamFormat(            \
               "[%s %s:%d] ",               \
               ::lighter::common::util::GetCurrentTime(), __FILE__, __LINE__)
#endif  // NDEBUG

#define LOG_INFO LOG(::std::cout)
#define LOG_ERROR LOG(::std::cerr)
#define LOG_SWITCH(is_error) (is_error ? LOG_ERROR : LOG_INFO)

#ifdef NDEBUG
#define FATAL(error) throw ::std::runtime_error{error}
#else  // !NDEBUG
#define FATAL(error)                              \
    throw ::std::runtime_error{::absl::StrFormat( \
        "%s() in %s at line %d: %s", __func__, __FILE__, __LINE__, error)}
#endif  // NDEBUG

#define ASSERT_TRUE(expr, error) if (!ABSL_PREDICT_TRUE(expr)) FATAL(error)

#define ASSERT_FALSE(expr, error) ASSERT_TRUE(!(expr), error)

#define ASSERT_HAS_VALUE(object, error) ASSERT_TRUE(object.has_value(), error)

#define ASSERT_NO_VALUE(object, error) ASSERT_FALSE(object.has_value(), error)

#define ASSERT_NON_NULL(pointer, error) ASSERT_FALSE(pointer == nullptr, error)

#define ASSERT_EMPTY(container, error) ASSERT_TRUE(container.empty(), error)

#define ASSERT_NON_EMPTY(container, error)        \
    ASSERT_FALSE(container.empty(), error)

#define FATAL_IF_NULL(pointer)                    \
    (ABSL_PREDICT_TRUE(pointer != nullptr)        \
        ? pointer                                 \
        : FATAL(#pointer " is nullptr"))

namespace lighter::common::util {

// Returns the current time in "YYYY-MM-DD HH:MM:SS.fff" format.
std::string GetCurrentTime();

// This class simply wraps a std::ostream. When an instance of it gets
// destructed, it will append std::endl to the end;
class Logger {
 public:
  explicit Logger(std::ostream& os) : os_{os} {}

  // This class is only move-constructible.
  Logger(Logger&&) noexcept = default;
  Logger& operator=(Logger&&) = delete;

  ~Logger() { os_ << std::endl; }

  template <typename Streamable>
  Logger& operator<<(const Streamable& streamable) {
    os_ << streamable;
    return *this;
  }

 private:
  std::ostream& os_;
};

namespace internal {

template <typename ContainerType>
std::optional<int> GetIndexIfExists(
    const ContainerType& container,
    typename ContainerType::const_iterator iter) {
  if (iter == container.end()) {
    return std::nullopt;
  }
  return std::distance(container.begin(), iter);
}

}  // namespace internal

// Returns true if 'container' contains 'target'.
template <typename ValueType, typename TargetType>
bool Contains(absl::Span<const ValueType> container, const TargetType& target) {
  const auto iter = std::find(container.begin(), container.end(), target);
  return iter != container.end();
}

// Returns the index of the first element that is equal to 'target'.
// If there is no such element, returns std::nullopt.
template <typename ValueType, typename TargetType>
std::optional<int> FindIndexOfFirst(absl::Span<const ValueType> container,
                                    const TargetType& target) {
  const auto iter = std::find(container.begin(), container.end(), target);
  return internal::GetIndexIfExists(container, iter);
}

// Returns the index of the first element that satisfies 'predicate'.
// If there is no such element, returns std::nullopt.
template <typename ValueType>
std::optional<int> FindIndexOfFirstIf(
    absl::Span<const ValueType> container,
    absl::FunctionRef<bool(const ValueType&)> predicate) {
  const auto iter = std::find_if(container.begin(), container.end(), predicate);
  return internal::GetIndexIfExists(container, iter);
}

// Moves 'element' to the specified 'index' of 'container'. 'container' will be
// resized if necessary.
template <typename ValueType>
void SetElementWithResizing(ValueType&& element, int index,
                            std::vector<ValueType>& container) {
  if (index >= container.size()) {
    container.resize(index + 1);
  }
  container[index] = std::forward<ValueType>(element);
}

// Removes duplicated elements from 'container' in-place, hence the size of
// 'container' may change if there exists any duplicate.
template <typename ValueType>
void RemoveDuplicate(std::vector<ValueType>& container) {
  if (container.size() > 1) {
    std::sort(container.begin(), container.end());
    const auto new_end = std::unique(container.begin(), container.end());
    container.resize(std::distance(container.begin(), new_end));
  }
}

// Returns the total data size of `container`.
template <typename ValueType>
size_t GetTotalDataSize(absl::Span<const ValueType> container) {
  return sizeof(ValueType) * container.size();
}

// Moves all elements of 'src' to the end of 'dst'.
template <typename ValueType>
void VectorAppend(std::vector<ValueType>& dst, std::vector<ValueType>& src) {
  dst.reserve(src.size() + dst.size());
  std::move(src.begin(), src.end(), std::back_inserter(dst));
  src.clear();
}

// Erases elements in 'container' that satisfies the 'predicate'.
template <typename ContainerType, typename PredicateType>
void EraseIf(const PredicateType& predicate, ContainerType& container) {
  auto iter = container.begin();
  while (iter != container.end()) {
    if (predicate(*iter)) {
      container.erase(iter++);
    } else {
      ++iter;
    }
  }
}

// Applies 'transform' to elements in 'container' and returns a vector of
// resulting objects.
template <typename SrcType, typename DstType>
std::vector<DstType> TransformToVector(
    absl::Span<const SrcType> container,
    absl::FunctionRef<DstType(const SrcType&)> transform) {
  std::vector<DstType> transformed;
  transformed.reserve(container.size());
  std::transform(container.begin(), container.end(),
                 std::back_inserter(transformed), transform);
  return transformed;
}

// Applies 'transform' to elements in 'container' and returns a hash set of
// resulting objects.
template <typename SrcType, typename DstType>
absl::flat_hash_set<DstType> TransformToSet(
    absl::Span<const SrcType> container,
    absl::FunctionRef<DstType(const SrcType&)> transform) {
  absl::flat_hash_set<DstType> transformed;
  std::transform(container.begin(), container.end(),
                 std::inserter(transformed, transformed.end()), transform);
  return transformed;
}

// Copies elements in 'container' that satisfies 'predicate' to a new vector.
template <typename ValueType>
std::vector<ValueType> CopyToVectorIf(
    absl::Span<const ValueType> container,
    absl::FunctionRef<bool(const ValueType&)> predicate) {
  std::vector<ValueType> copied;
  std::copy_if(container.begin(), container.end(), std::back_inserter(copied),
               predicate);
  return copied;
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

// Reduces all elements in 'container' to one value of AccumulatorType.
template <typename AccumulatorType, typename ContainerType>
AccumulatorType Reduce(
    const ContainerType& container,
    absl::FunctionRef<AccumulatorType(
        const typename ContainerType::value_type&)> extract_value) {
  return std::accumulate(
      container.begin(), container.end(), AccumulatorType{},
      [extract_value](const AccumulatorType& accumulated,
                      const typename ContainerType::value_type& element) {
        return accumulated + extract_value(element);
      });
}

// Returns whether
inline bool IsPowerOf2(unsigned int x) {
  if (x <= 0) {
    return false;
  }
  return (x & (x - 1)) == 0;
}

// Helper class to enable using an enum class as the key of a hash map:
//   absl::flat_hash_map<KeyType, ValueType, EnumClassHash>;
struct EnumClassHash {
  template <typename EnumClass>
  std::size_t operator()(EnumClass value) const {
    return static_cast<std::size_t>(value);
  }
};

// Helper class to enable using std::filesystem::path as the key of a hash map.
struct PathHash {
  std::size_t operator()(const std::filesystem::path& path) const {
    return std::filesystem::hash_value(path);
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

}  // namespace internal

// Returns a pointer of 'ExpectedType'. If elements of 'container' are of
// 'ExpectedType', the returned pointer will point to its underlying data.
// Otherwise, returns nullptr.
template <typename ExpectedType, typename ValueType>
const ExpectedType* GetPointerIfTypeExpected(
    const std::vector<ValueType>& container) {
  return internal::GetPointerIfTypeExpectedImpl<ExpectedType, ValueType>(
      container.data(), std::is_same<ExpectedType, ValueType>());
}

// Includes 'to_include' in 'value' using operator |= if 'condition' is true.
template <typename ValueType, typename IncludeType>
void IncludeIfTrue(bool condition, ValueType& value, IncludeType to_include) {
  if (condition) {
    value |= to_include;
  }
}

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_UTIL_H
