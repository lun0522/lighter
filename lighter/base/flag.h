#ifndef LIGHTER_BASE_FLAG_H
#define LIGHTER_BASE_FLAG_H

#include <string>
#include <string_view>

namespace lighter::base::flag {

// Registers and stores a typed command-line flag.
template <typename T>
class Flag {
 public:
  Flag(std::string_view name, T default_value, std::string_view description);

  // This class is neither copyable nor movable.
  Flag(const Flag&) = delete;
  Flag& operator=(const Flag&) = delete;

  const T& Get() const { return value_; }
  void Set(T value) { value_ = std::move(value); }
  std::string_view name() const { return name_; }
  std::string_view description() const { return description_; }

 private:
  std::string_view name_;
  T value_;
  std::string_view description_;
};

// Parses --<flag>=<value>, --<flag> <value>, --<bool_flag>, --no<bool_flag>,
// and --help from command-line arguments.
void ParseCommandLine(int argc, char* argv[]);

}  // namespace lighter::base::flag

#define LIGHTER_FLAG(type, name, default_value, description) \
  ::lighter::base::flag::Flag<type> FLAGS_##name {           \
    #name, default_value, description                        \
  }

#define LIGHTER_DECLARE_FLAG(type, name) \
  extern ::lighter::base::flag::Flag<type> FLAGS_##name

#endif  // LIGHTER_BASE_FLAG_H
