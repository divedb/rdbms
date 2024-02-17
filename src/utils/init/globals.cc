#include "rdbms/utils/globals.hpp"

namespace rdbms {

std::atomic_bool g_interrupt_pending;
std::atomic_bool g_query_cancel_pending;
std::atomic_bool g_proc_die_pending;
std::atomic_bool g_immediate_interrupt_ok;
std::atomic_uint32_t g_interrupt_hold_off_count;
std::atomic_uint32_t g_crit_section_count;

int g_debug_level;

// ipc.cc
bool g_proc_exit_inprogress;

void global_var_init() {
  g_interrupt_pending.store(false, std::memory_order_release);
  g_query_cancel_pending.store(false, std::memory_order_release);
  g_proc_die_pending.store(false, std::memory_order_release);
  g_immediate_interrupt_ok.store(false, std::memory_order_release);

  g_interrupt_hold_off_count.store(0, std::memory_order_release);
  g_crit_section_count.store(0, std::memory_order_release);

  g_debug_level = 0;
  g_proc_exit_inprogress = false;
}

}  // namespace rdbms