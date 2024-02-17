#include "rdbms/utils/mcxt.hpp"

namespace rdbms {

MemoryContextData::MemoryContextData(NodeTag type, MemoryContext parent,
                                     std::string name)
    : type_(type),
      parent_(parent),
      name_(std::move(name)),
      first_child_{nullptr},
      next_sibling_{nullptr} {
  if (parent_) {
    next_sibling_ = parent_->first_child_;
    parent_->first_child_ = this;
  }
}

void MemoryContextData::reset_subtree() {
  reset_subtree(first_child_);
  reset();
}

void MemoryContextData::reset_subtree(MemoryContext context) {
  if (context == nullptr) {
    return;
  }

  reset_subtree(context->first_child_);
  reset_subtree(context->next_sibling_);
  context->reset();
}

void MemoryContextData::destroy_subtree() {
  destroy_subtree(first_child_);
  destroy();
}

void MemoryContextData::destroy_subtree(MemoryContext context) {
  if (context == nullptr) {
    return;
  }

  destroy_subtree(context->first_child_);
  destroy_subtree(context->next_sibling_);
  context->destroy();
}

}  // namespace rdbms
