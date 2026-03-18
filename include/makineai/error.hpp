#pragma once

#include <string>
#include <expected>

namespace makineai {

enum class ErrorCode {
    OK = 0,
    FileNotFound,
    FileAccessDenied,
    InvalidFormat,
    InvalidArgument,
    InvalidOffset,
    ParseError,
    DecompressionFailed,
    NotSupported,
    NotImplemented,
    UnsupportedVersion,
    NetworkError,
    IOError,
    Unknown,
};

struct Error {
    ErrorCode code;
    std::string message;

    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
};

template<typename T>
using Result = std::expected<T, Error>;

using VoidResult = std::expected<void, Error>;

} // namespace makineai
