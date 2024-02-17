#include <string>
#include <vector>

#include "rdbms/parser/scan_escape.h"

#include <gtest/gtest.h>

struct TestCase {
  const char* cstr;
  std::string expect;
};

TEST(ScanEscape, Basic) {
  std::vector<TestCase> tests = {{"abc", "abc"},
                                 {"\\r\\f\\t\\n\\b", "\r\f\t\n\b"},
                                 {"\\61abc", "1abc"},
                                 {"\\141abc", "aabc"},
                                 {"\\", std::string(1, '\0')}};

  for (auto&& test : tests) {
    std::string actual = rdbms::scan_escape(test.cstr);
    EXPECT_EQ(test.expect, actual);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}