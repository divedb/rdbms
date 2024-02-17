#pragma once

#include <cstdarg>
#include <cstdio>

namespace rdbms {

#define NOTICE       0     // Random info - no special action
#define ERROR        (-1)  // User error - return to known state
#define FATAL        1     // Fatal error - abort process
#define REALLY_FATAL 2     // Take down the other backends with me
#define STOP         REALLY_FATAL
#define DEBUG        (-2)  // Debug message
#define LOG          DEBUG
#define NOIND        (-3)  // Debug message, don't indent as far

// TODO(gc): fix later.
inline void elog(int level, const char* fmt, ...) {
  static char buf[1024];

  va_list ap;

  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);

  buf[n] = '\0';

  printf("%s\n", buf);
}

}  // namespace rdbms