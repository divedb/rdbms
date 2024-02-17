#pragma once

#include "rdbms/postgres.hpp"
#include <type_traits>

namespace rdbms {

// template <Size N>
// struct Log2 : std::integral_constant<Size,;

// template <>
// struct Log2<1> : std::integral_constant<Size, 0> {};

// template <>
// struct Log2<0> {};

// https://stackoverflow.com/questions/3272424/compute-fast-log-base-2-ceiling
int ceil_log2(Size x) {
  static Size t[] = {0xFFFFFFFF00000000ull, 0x00000000FFFF0000ull,
                     0x000000000000FF00ull, 0x00000000000000F0ull,
                     0x000000000000000Cull, 0x0000000000000002ull};

  int y = (((x & (x - 1)) == 0) ? 0 : 1);
  int j = 32;
  int i;

  for (i = 0; i < 6; i++) {
    int k = (((x & t[i]) == 0) ? 0 : j);
    y += k;
    x >>= k;
    j >>= 1;
  }

  return y;
}

}  // namespace rdbms