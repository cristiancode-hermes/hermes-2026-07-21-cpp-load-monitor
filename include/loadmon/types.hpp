#pragma once

#include <expected>
#include <string>

namespace loadmon {

struct Error {
  int code;
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

} // namespace loadmon
