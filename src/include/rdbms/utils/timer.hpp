#pragma once

#include <chrono>

namespace rdbms {

class Timer {
 public:
  Timer() : start_(std::chrono::high_resolution_clock::now()) {}

  template <typename Duration = std::chrono::milliseconds>
  double elapsed() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<Duration>(end - start_).count();
  }

  void reset() { start_ = std::chrono::high_resolution_clock::now(); }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

}  // namespace rdbms