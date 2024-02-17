#pragma once

#include <string>

#include "rdbms/utils/string_piece.hpp"
#include "rdbms/utils/types.hpp"

namespace rdbms {

namespace detail {

inline constexpr int kSmallBuffer = 4000;
inline constexpr int kLargeBuffer = 4000 * 1000;

template <int Size>
class FixedBuffer {
 public:
  FixedBuffer() : cur_(data_) { set_cookie(cookie_start); }

  ~FixedBuffer() { set_cookie(cookie_end); }

  FixedBuffer(const FixedBuffer&) = delete;
  FixedBuffer& operator=(const FixedBuffer&) = delete;

  void append(const char* buf, std::size_t len) {
    // FIXME: append partially
    if (implicit_cast<size_t>(avail()) > len) {
      memcpy(cur_, buf, len);
      cur_ += len;
    }
  }

  const char* data() const { return data_; }
  int length() const { return static_cast<int>(cur_ - data_); }

  // Write to data_ directly.
  char* current() { return cur_; }
  int avail() const { return static_cast<int>(end() - cur_); }
  void add(std::size_t len) { cur_ += len; }

  void reset() { cur_ = data_; }
  void bzero() { memset(data_, 0, sizeof(data_)); }

  // For used by GDB.
  const char* debug_string();
  void set_cookie(void (*cookie)()) { cookie_ = cookie; }

  // For used by unit test.
  std::string to_string() const { return std::string(data_, length()); }
  StringPiece to_string_piece() const { return StringPiece(data_, length()); }

 private:
  const char* end() const { return data_ + sizeof(data_); }

  // Must be outline function for cookies.
  static void cookie_start();
  static void cookie_end();

  void (*cookie_)();
  char data_[Size];
  char* cur_;
};

}  // namespace detail

class LogStream {
  typedef LogStream self;

 public:
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

  LogStream(const LogStream&) = delete;
  LogStream& operator=(const LogStream&) = delete;

  self& operator<<(bool v) {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  self& operator<<(const void*);

  self& operator<<(float v) {
    *this << static_cast<double>(v);
    return *this;
  }
  self& operator<<(double);
  // self& operator<<(long double);

  self& operator<<(char v) {
    buffer_.append(&v, 1);
    return *this;
  }

  // self& operator<<(signed char);
  // self& operator<<(unsigned char);

  self& operator<<(const char* str) {
    if (str) {
      buffer_.append(str, strlen(str));
    } else {
      buffer_.append("(null)", 6);
    }
    return *this;
  }

  self& operator<<(const unsigned char* str) {
    return operator<<(reinterpret_cast<const char*>(str));
  }

  self& operator<<(const string& v) {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

  self& operator<<(const StringPiece& v) {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  self& operator<<(const Buffer& v) {
    *this << v.to_string_piece();
    return *this;
  }

  void append(const char* data, int len) { buffer_.append(data, len); }
  const Buffer& buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

 private:
  void static_check();

  template <typename T>
  void formatInteger(T);

  Buffer buffer_;

  static const int kMaxNumericSize = 48;
};

class Fmt {
 public:
  template <typename T>
  Fmt(const char* fmt, T val);

  const char* data() const { return buf_; }
  int length() const { return length_; }

 private:
  char buf_[32];
  int length_;
};

inline LogStream& operator<<(LogStream& s, const Fmt& fmt) {
  s.append(fmt.data(), fmt.length());
  return s;
}

// Format quantity n in SI units (k, M, G, T, P, E).
// The returned string is atmost 5 characters long.
// Requires n >= 0
std::string formatSI(int64_t n);

// Format quantity n in IEC (binary) units (Ki, Mi, Gi, Ti, Pi, Ei).
// The returned string is atmost 6 characters long.
// Requires n >= 0
std::string formatIEC(int64_t n);

}  // namespace rdbms