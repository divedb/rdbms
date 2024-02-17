#pragma once

namespace rdbms {

#define ENABLE_SYSLOG       1
#define DEF_MAX_BACKENDS    32
#define USE_ASSERT_CHECKING 1

#define MAX_BACKENDS   (DEF_MAX_BACKENDS > 1024 ? DEF_MAX_BACKENDS : 1024)
#define DEF_NBUFFERS   (DEF_MAX_BACKENDS > 8 ? DEF_MAX_BACKENDS * 2 : 16)
#define BLCKSZ         8192
#define RELSEG_SIZE    (0x40000000 / BLCKSZ)
#define INDEX_MAX_KEYS 16
#define FUNC_MAX_ARGS  INDEX_MAX_KEYS

#define USER_LOCKS
#define FAST_BUILD
#define MAX_PG_PATH            1024
#define DEFAULT_MAX_EXPR_DEPTH 10000
#define BITS_PER_BYTE          8
#define DEFAULT_PGSOCKET_DIR   "/tmp"

}  // namespace rdbms