#pragma once

#include <climits>
#include <cstdint>

#include "rdbms/c.hpp"

namespace rdbms {

#define IGNORE(var) ((void)var)

enum class RunMode : std::uint8_t { kBootstrap, kNormal };

template <RunMode Mode>
class Postgres {};

template <>
class Postgres<RunMode::kBootstrap>;

}  // namespace rdbms