#pragma once

#include <cassert>
#include <vector>

#include "rdbms/postgres.hpp"
#include "rdbms/utils/mcxt.hpp"

namespace rdbms {

enum class MemCxtType : i8 {
  kTopMemoryContext = 0,
  kErrorContext = 1,
  kPostmasterContext = 2,
  kCacheMemoryContext = 3,
  kQueryContext = 4,
  kTopTransactionContext = 5,
  kTansactionCommandContext = 6,
  kDynHashContext = 7,

  kNoContexts
};

class MemoryManager {
 public:
  static MemoryContext context(MemCxtType cxt_type) {
    assert(cxt_type < MemCxtType::kNoContexts);

    return contexts_[static_cast<int>(cxt_type)];
  }

 private:
  static MemCxtType cur_context_type_;
  static std::vector<MemoryContext> contexts_;
};

}  // namespace rdbms