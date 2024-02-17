#include "rdbms/postgres.h"

namespace rdbms {

template <>
class Postgres<RunMode::kBootstrap> {};

}  // namespace rdbms