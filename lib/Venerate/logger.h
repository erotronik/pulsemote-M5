#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdio>

// Signature: user_ctx + already-formatted string
using log_write_fn = void (*)(void* user_ctx, const char* msg);

class Logger {
public:
  void set(log_write_fn fn, void* ctx = nullptr) {
    write_fn_ = fn;
    ctx_ = ctx;
  }

  void clear() { write_fn_ = nullptr; ctx_ = nullptr; }

  void printf(const char* fmt, ...) {
    if (!write_fn_) return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n <= 0) return;
    buf[sizeof(buf) - 1] = '\0';
    write_fn_(ctx_, buf);
  }

private:
  log_write_fn write_fn_ = nullptr;
  void* ctx_ = nullptr;
};
