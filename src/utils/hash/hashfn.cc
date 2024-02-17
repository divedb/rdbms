#include "rdbms/postgres.hpp"

namespace rdbms {

inline constexpr int kPrime1 = 37;
inline constexpr int kPrime2 = 1048583;

Size string_hash(const char* key, int size) {
  int h = 0;
  auto k = reinterpret_cast<const unsigned char*>(key);

  while (*k) {
    h = h * kPrime1 ^ (*k++ - ' ');
  }

  h %= kPrime2;

  return h;
}

Size tag_hash(const char* key, int size) {
  Size h = 0;
  auto k = reinterpret_cast<const int*>(key);

  // Convert tag to integer; Use four byte chunks in a "jump table" to
  // go a little faster. Currently the maximum keysize is 16 (mar 17
  // 1992) I have put in cases for up to 24. Bigger than this will
  // resort to the old behavior of the for loop. (see the default case).
  switch (size) {
    case 6 * sizeof(int):
      h = h * kPrime1 ^ (*k);
      k++;

    case 5 * sizeof(int):
      h = h * kPrime1 ^ (*k);
      k++;

    case 4 * sizeof(int):
      h = h * kPrime1 ^ (*k);
      k++;

    case 3 * sizeof(int):
      h = h * kPrime1 ^ (*k);
      k++;

    case 2 * sizeof(int):
      h = h * kPrime1 ^ (*k);
      k++;

    case sizeof(int):
      h = h * kPrime1 ^ (*k);
      k++;
      break;

    default:
      for (; size >= (int)sizeof(int); size -= sizeof(int), k++) {
        h = h * kPrime1 ^ (*k);
      }

      // Now let's grab the last few bytes of the tag if the tag has
      // (size % 4) != 0 (which it sometimes will on a sun3).
      if (size) {
        auto keytmp = reinterpret_cast<const char*>(k);

        switch (size) {
          case 3:
            h = h * kPrime1 ^ (*keytmp);
            keytmp++;

          case 2:
            h = h * kPrime1 ^ (*keytmp);
            keytmp++;

          case 1:
            h = h * kPrime1 ^ (*keytmp);
            break;
        }
      }

      break;
  }

  h %= kPrime2;

  return h;
}

}  // namespace rdbms