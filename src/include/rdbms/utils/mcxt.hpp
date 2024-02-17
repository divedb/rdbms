#pragma once

#include <string>

#include "rdbms/nodes/nodes.hpp"

namespace rdbms {

using MemoryContext = class MemoryContextData*;

class MemoryContextData {
 public:
  MemoryContextData(NodeTag type, MemoryContext parent, std::string name);
  virtual ~MemoryContextData();

  virtual void* alloc(Size size);
  virtual void free(void* pointer);
  virtual void* realloc(void* pointer, Size size);
  virtual void reset();
  virtual void destroy();
  virtual void check();
  virtual void stats();

  NodeTag type() const { return type_; }
  std::string name() const { return name_; }

  // Release all space allocated within a context and its descendants,
  // but don't delete the contexts themselves.
  void reset_subtree();

  // Release all space allocated within a context and delete all
  // its descendants.
  void destroy_subtree();

 protected:
  void reset_subtree(MemoryContext context);
  void destroy_subtree(MemoryContext context);

  NodeTag type_;
  MemoryContext parent_;
  MemoryContext first_child_;
  MemoryContext next_sibling_;
  std::string name_;
};

// extern MemoryContext top_memory_context;
// extern MemoryContext error_context;
// extern MemoryContext postmaster_context;
// extern MemoryContext cache_memory_context;
// extern MemoryContext query_context;
// extern MemoryContext top_transaction_context;
// extern MemoryContext transaction_command_context;

void memory_context_init();

}  // namespace rdbms