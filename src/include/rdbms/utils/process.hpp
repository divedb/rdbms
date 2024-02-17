#pragma once

namespace rdbms {

class Process {
 public:
  static void fork();
  static void getpid();
  static void exit();
};

}  // namespace rdbms