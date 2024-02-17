#include <cstdlib>
#include <string>

#include "rdbms/parser/scan_escape.h"

namespace rdbms {

std::string scan_escape(const char* cstr) {
  if (cstr == nullptr || cstr[0] == '\0') {
    return "";
  }

  std::string ret;
  auto size = strlen(cstr);

  ret.reserve(size);

  for (auto i = 0; i < size; i++) {
    char c = cstr[i];

    if (c == '\'') {
      i++;
      ret.push_back(cstr[i]);
    } else if (c == '\\') {
      i++;

      switch (cstr[i]) {
        case 'b':
          ret.push_back('\b');
          break;
        case 'f':
          ret.push_back('\f');
          break;
        case 'n':
          ret.push_back('\n');
          break;
        case 'r':
          ret.push_back('\r');
          break;
        case 't':
          ret.push_back('\t');
          break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7': {
          int k;
          long oct_val = 0;

          for (k = 0; cstr[k] >= '0' && cstr[i + k] <= '7' && k < 3; k++) {
            oct_val = (oct_val << 3) + (cstr[i + k] - '0');
          }

          i += k - 1;
          ret.push_back(static_cast<char>(oct_val));
          break;
        }

        default:
          // TODO(gc): do we need to add this invalid character?
          ret.push_back(cstr[i]);
          break;
      }
    } else {
      ret.push_back(c);
    }
  }

  return ret;
}

}  // namespace rdbms