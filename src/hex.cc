//
// Created by kilingzhang on 2021/7/31.
// copy from https://github.com/rnburn/zipkin-cpp-opentracing
//

#include "include/hex.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <cstring>
#include <limits>
#include <climits>
#include <vector>

std::string Hex::encode(const uint8_t *data, size_t length) {
  static const char *const digits = "0123456789abcdef";

  std::string ret;
  ret.reserve(length * 2);

  for (size_t i = 0; i < length; i++) {
    uint8_t d = data[i];
    ret.push_back(digits[d >> 4]);
    ret.push_back(digits[d & 0xf]);
  }

  return ret;
}

bool atoul(const char *str, uint64_t &out, int base) {
  if (strlen(str) == 0) {
    return false;
  }

  char *end_ptr;
  out = strtoul(str, &end_ptr, base);
  if (*end_ptr != '\0' || (out == ULONG_MAX && errno == ERANGE)) {
    return false;
  } else {
    return true;
  }
}

std::vector<uint8_t> Hex::decode(const std::string &hex_string) {
  if (hex_string.empty()) {
    return {};
  }
  auto num_bytes = hex_string.size() / 2 + (hex_string.size() % 2 > 0);
  std::vector<uint8_t> result;
  result.reserve(num_bytes);

  std::vector<uint8_t> segment;
  segment.reserve(num_bytes);
  size_t i = 0;
  std::string hex_byte;
  hex_byte.reserve(2);
  uint64_t out;

  if (hex_string.size() % 2 == 1) {
    hex_byte = hex_string.substr(0, 1);
    if (!atoul(hex_byte.c_str(), out, 16)) {
      return {};
    }
    segment.push_back(out);
    i = 1;
  }

  for (; i < hex_string.size(); i += 2) {
    hex_byte = hex_string.substr(i, 2);
    if (!atoul(hex_byte.c_str(), out, 16)) {
      return {};
    }

    segment.push_back(out);
  }

  return segment;
}
