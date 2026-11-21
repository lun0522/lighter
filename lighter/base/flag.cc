#include "lighter/base/flag.h"

#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "lighter/base/util.h"

namespace lighter::base::flag {
namespace {

struct FlagEntry {
  std::string description;
  bool is_bool;
  std::function<void(std::string_view)> parse;
};

auto& GetRegistry() {
  static auto* registry =
      new std::unordered_map<std::string, FlagEntry,
                             util::TransparentStringHash, std::equal_to<>>{};
  return *registry;
}

bool ParseBool(std::string_view text) {
  if (text == "true" || text == "1" || text == "yes") {
    return true;
  }
  if (text == "false" || text == "0" || text == "no") {
    return false;
  }
  FATAL(std::format("Invalid boolean flag value '{}'", text));
}

void PrintHelpAndExit(std::string_view program_name) {
  std::cout << "Usage: " << program_name << " [flags]\n\nFlags:\n";
  for (const auto& [name, entry] : GetRegistry()) {
    std::cout << "  --" << name << ": " << entry.description << '\n';
  }
  std::exit(EXIT_SUCCESS);
}

}  // namespace

template <>
Flag<bool>::Flag(std::string_view name, bool default_value,
                 std::string_view description)
    : name_{name}, value_{default_value}, description_{description} {
  GetRegistry().emplace(std::string{name}, FlagEntry{
                                               std::string{description},
                                               /*is_bool=*/true,
                                               [this](std::string_view val) {
                                                 value_ = ParseBool(val);
                                               },
                                           });
}

template <>
Flag<std::string>::Flag(std::string_view name, std::string default_value,
                        std::string_view description)
    : name_{name}, value_{std::move(default_value)}, description_{description} {
  GetRegistry().emplace(std::string{name}, FlagEntry{
                                               std::string{description},
                                               /*is_bool=*/false,
                                               [this](std::string_view val) {
                                                 value_ = std::string{val};
                                               },
                                           });
}

void ParseCommandLine(int argc, char* argv[]) {
  const auto& registry = GetRegistry();
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg == "--help" || arg == "-h") {
      PrintHelpAndExit(argc > 0 ? argv[0] : "program");
    }
    if (!arg.starts_with("--")) {
      continue;
    }

    const std::string_view body = arg.substr(2);
    const size_t eq_pos = body.find('=');
    const std::string_view name =
        eq_pos == std::string_view::npos ? body : body.substr(0, eq_pos);

    if (eq_pos != std::string_view::npos) {
      const auto iter = registry.find(name);
      ASSERT_TRUE(iter != registry.end(),
                  std::format("Unknown flag '--{}'", name));
      iter->second.parse(body.substr(eq_pos + 1));
      continue;
    }

    if (const auto iter = registry.find(name); iter != registry.end()) {
      if (iter->second.is_bool) {
        iter->second.parse("true");
      } else {
        ASSERT_TRUE(i + 1 < argc,
                    std::format("Flag '--{}' requires a value", name));
        iter->second.parse(argv[++i]);
      }
      continue;
    }

    if (name.starts_with("no")) {
      const std::string_view pos_name = name.substr(2);
      const auto iter = registry.find(pos_name);
      if (iter != registry.end() && iter->second.is_bool) {
        iter->second.parse("false");
        continue;
      }
    }

    FATAL(std::format("Unknown flag '--{}'", name));
  }
}

}  // namespace lighter::base::flag
