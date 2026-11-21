//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_BASE_UTIL_H
#define LIGHTER_BASE_UTIL_H

#include <algorithm>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <vector>

#ifdef NDEBUG
#define LOG(stream)                     \
  ::lighter::base::util::Logger{stream} \
      << ::std::format("{} ", ::lighter::base::util::GetCurrentTime())
#else  // !NDEBUG
#define LOG(stream)                                                            \
  ::lighter::base::util::Logger{stream}                                        \
      << ::std::format("[{} {}:{}] ", ::lighter::base::util::GetCurrentTime(), \
                       __FILE__, __LINE__)
#endif  // NDEBUG

#define LOG_INFO LOG(::std::cout)
#define LOG_ERROR LOG(::std::cerr)
#define LOG_SWITCH(is_error) (is_error ? LOG_ERROR : LOG_INFO)

#ifdef NDEBUG
#define FATAL(error) \
  throw ::std::runtime_error { error }
#else  // !NDEBUG
#define FATAL(error)                                                         \
  throw ::std::runtime_error {                                               \
    ::std::format("{}() in {} at line {}: {}", __func__, __FILE__, __LINE__, \
                  error)                                                     \
  }
#endif  // NDEBUG

#define ASSERT_TRUE(expr, error) \
  if (!(expr)) FATAL(error)

#define ASSERT_FALSE(expr, error) ASSERT_TRUE(!(expr), error)

#define ASSERT_HAS_VALUE(object, error) ASSERT_TRUE(object.has_value(), error)

#define ASSERT_NO_VALUE(object, error) ASSERT_FALSE(object.has_value(), error)

#define ASSERT_NON_NULL(pointer, error) ASSERT_FALSE(pointer == nullptr, error)

#define ASSERT_EMPTY(container, error) ASSERT_TRUE(container.empty(), error)

#define ASSERT_NON_EMPTY(container, error) \
  ASSERT_FALSE(container.empty(), error)

#define FATAL_IF_NULL(pointer) \
  ((pointer) != nullptr ? (pointer) : FATAL(#pointer " is nullptr"))

namespace lighter::base::util {

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

template <typename ContainerType, typename IterType>
std::optional<int> GetIndexIfExists(const ContainerType& container,
                                    IterType iter) {
  if (iter == container.end()) {
    return std::nullopt;
  }
  return static_cast<int>(std::distance(container.begin(), iter));
}

}  // namespace internal

// Transparent hash allowing heterogeneous lookup with std::string_view.
struct TransparentStringHash {
  using is_transparent = void;

  std::size_t operator()(std::string_view text) const {
    return std::hash<std::string_view>{}(text);
  }
};

// Splits 'text' by 'delimiter', optionally skipping empty segments.
std::vector<std::string> StrSplit(std::string_view text, char delimiter,
                                  bool skip_empty = false);

// Returns true if 'container' contains 'target'.
template <typename ValueType, typename TargetType>
bool Contains(std::span<const ValueType> container, const TargetType& target) {
  const auto iter = std::find(container.begin(), container.end(), target);
  return iter != container.end();
}

// Returns the index of the first element that satisfies 'predicate'.
// If there is no such element, returns std::nullopt.
template <typename ValueType>
std::optional<int> FindIndexOfFirstIf(
    std::span<const ValueType> container,
    const std::function<bool(const ValueType&)>& predicate) {
  const auto iter = std::find_if(container.begin(), container.end(), predicate);
  return internal::GetIndexIfExists(container, iter);
}

// Returns the total data size of `container`.
template <typename ValueType>
size_t GetTotalDataSize(std::span<const ValueType> container) {
  return sizeof(ValueType) * container.size();
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

// Applies 'transform' to elements in 'container' and returns a hash set of
// resulting objects.
template <typename SrcType, typename DstType>
std::unordered_set<DstType> TransformToSet(
    std::span<const SrcType> container,
    const std::function<DstType(const SrcType&)>& transform) {
  std::unordered_set<DstType> transformed;
  std::transform(container.begin(), container.end(),
                 std::inserter(transformed, transformed.end()), transform);
  return transformed;
}

// Copies elements in 'container' that satisfies 'predicate' to a new vector.
template <typename ValueType>
std::vector<ValueType> CopyToVectorIf(
    std::span<const ValueType> container,
    const std::function<bool(const ValueType&)>& predicate) {
  std::vector<ValueType> copied;
  std::copy_if(container.begin(), container.end(), std::back_inserter(copied),
               predicate);
  return copied;
}

// Reduces all elements in 'container' to one value of AccumulatorType.
template <typename AccumulatorType, typename ContainerType>
AccumulatorType Reduce(
    const ContainerType& container,
    const std::function<AccumulatorType(
        const typename ContainerType::value_type&)>& extract_value) {
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
//   std::unordered_map<KeyType, ValueType, EnumClassHash>;
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

// Includes 'to_include' in 'value' using operator |= if 'condition' is true.
template <typename ValueType, typename IncludeType>
void IncludeIfTrue(bool condition, ValueType& value, IncludeType to_include) {
  if (condition) {
    value |= to_include;
  }
}

}  // namespace lighter::base::util

#endif  // LIGHTER_BASE_UTIL_H
