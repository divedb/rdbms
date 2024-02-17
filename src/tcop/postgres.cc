#include "rdbms/utils/globals.hpp"

namespace rdbms {
// 参考文献
// https://enteros.com/database-performance-management/postgresql-query/
//
// ProcessInterrupts: out-of-line portion of CHECK_FOR_INTERRUPTS() macro
//
// If an interrupt condition is pending, and it's safe to service it,
// then clear the flag and accept the interrupt. Called only when
// InterruptPending is true.
void process_interrupts() {
  // TODO:
  if (LOAD(g_interrupt_hold_off_count) != 0 ||
      LOAD(g_crit_section_count) != 0) {
    return;
  }

  // 表示在处理中断 没有pending
  STORE(g_interrupt_pending, false);

  if (LOAD(g_proc_die_pending)) {
    STORE(g_proc_die_pending, false);
    STORE(g_query_cancel_pending, false);
    STORE(g_immediate_interrupt_ok, false);

    // TODO(gc): add log
  }

  if (LOAD(g_query_cancel_pending)) {
    STORE(g_query_cancel_pending, false);
    STORE(g_immediate_interrupt_ok, false);

    // TODO(gc): add log
  }
}

}  // namespace rdbms