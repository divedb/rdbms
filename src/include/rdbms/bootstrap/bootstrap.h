#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "rdbms/postgres.h"

namespace rdbms {

// The maximum number of attributes in a relation supported
// at bootstrap time (ie, the max possible in a system table).
#define MAX_ATTR 40

class StringTable {
 public:
  static constexpr int kStrTableSize = 10000;

  explicit StringTable() { str_table_.reserve(kStrTableSize); }

  // Returns the string table position of the identifie
  // passed to it. We add it to the table if we can't find it.
  int enter_string(const char* cstr) {
    std::string_view sv(cstr);

    if (auto pos = hash_table_.find(sv); pos != hash_table_.end()) {
      return pos->second;
    }

    std::string str(sv);

    // Some of the utilites (eg, define type, create relation) assume that
    // the string they're passed is a NAME_DATA_LEN. We get array bound
    // read violations from purify if we don't allocate at least
    // NAME_DATA_LEN bytes for strings of this sort. Because we're lazy, we
    // allocate at least NAME_DATA_LEN bytes all the time.
    if (str.size() < NAME_DATA_LEN - 1) {
      str.resize(NAME_DATA_LEN);
    }

    str_table_.push_back(str);
    int index = str_table_.size();
    hash_table_[str_table_.back()] = index;

    return index;
  }

 private:
  std::vector<std::string> str_table_;
  std::unordered_map<std::string_view, int> hash_table_;
};

}  // namespace rdbms