//
//  flag_test.cc
//

#include "lighter/base/flag.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

LIGHTER_FLAG(bool, test_bool, false, "Boolean flag for testing");
LIGHTER_FLAG(std::string, test_str, "default_val", "String flag for testing");

namespace lighter::base::flag {
namespace {

void RunParse(std::vector<std::string> args) {
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  ParseCommandLine(static_cast<int>(argv.size()), argv.data());
}

class FlagTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_test_bool.Set(false);
    FLAGS_test_str.Set("default_val");
  }
};

TEST_F(FlagTest, ParsesBoolVariantsAndStringValues) {
  RunParse({"prog", "positional", "--test_bool", "--test_str", "hello"});
  EXPECT_TRUE(FLAGS_test_bool.Get());
  EXPECT_EQ(FLAGS_test_str.Get(), "hello");

  RunParse({"prog", "--notest_bool", "--test_str=world"});
  EXPECT_FALSE(FLAGS_test_bool.Get());
  EXPECT_EQ(FLAGS_test_str.Get(), "world");

  RunParse({"prog", "--test_bool=yes"});
  EXPECT_TRUE(FLAGS_test_bool.Get());
  RunParse({"prog", "--test_bool=0"});
  EXPECT_FALSE(FLAGS_test_bool.Get());
  RunParse({"prog", "--test_bool=1"});
  EXPECT_TRUE(FLAGS_test_bool.Get());
  RunParse({"prog", "--test_bool=no"});
  EXPECT_FALSE(FLAGS_test_bool.Get());
}

TEST_F(FlagTest, ThrowsOnInvalidOrMissingFlagValues) {
  EXPECT_THROW(RunParse({"prog", "--unknown_flag"}), std::runtime_error);
  EXPECT_THROW(RunParse({"prog", "--unknown_flag=1"}), std::runtime_error);
  EXPECT_THROW(RunParse({"prog", "--notest_str"}), std::runtime_error);
  EXPECT_THROW(RunParse({"prog", "--test_bool=maybe"}), std::runtime_error);
  EXPECT_THROW(RunParse({"prog", "--test_str"}), std::runtime_error);
}

}  // namespace
}  // namespace lighter::base::flag
