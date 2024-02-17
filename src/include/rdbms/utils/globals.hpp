#pragma once

#include <atomic>

namespace rdbms {

void process_interrupts();

#define STORE(var, value) var.store(value, std::memory_order_release)
#define LOAD(var)         var.load(std::memory_order_acquire)

#define CHECK_FOR_INTERRUPTS() \
  do {                         \
    if (g_interrupt_pending) { \
      process_interrupts();    \
    }                          \
  } while (0)

extern std::atomic_bool g_interrupt_pending;
extern std::atomic_bool g_query_cancel_pending;
extern std::atomic_bool g_proc_die_pending;
extern std::atomic_bool g_immediate_interrupt_ok;
extern std::atomic_uint32_t g_interrupt_hold_off_count;
extern std::atomic_uint32_t g_crit_section_count;

extern int g_debug_level;

// ipc.cc
extern bool g_proc_exit_inprogress;

void global_var_init();

}  // namespace rdbms