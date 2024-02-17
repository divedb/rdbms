#include <cassert>

#include "rdbms/nodes/nodes.h"

#include "rdbms/postgres.h"

namespace rdbms {

Node* new_node(Size size, NodeTag tag) {
  assert(size >= sizeof(Node));

  Node* new_node = (Node*)palloc(size);
}

}  // namespace rdbms