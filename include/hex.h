//
// Created by kilingzhang on 2021/7/31.
//

#ifndef OPENTELEMETRY_INCLUDE_HEX_H_
#define OPENTELEMETRY_INCLUDE_HEX_H_

#pragma once

#include <string>
#include <vector>

/**
 * Hex encoder/decoder. Produces lowercase hex digits. Can consume either
 * lowercase or uppercase digits.
 */
class Hex final {
 public:
  /**
   * Generates a hex dump of the given data
   * @param data the binary data to convert
   * @param length the length of the data
   * @return the hex encoded string representing data
   */
  static std::string encode(const uint8_t *data, size_t length);

  /**
   * Converts a hex dump to binary data
   * @param input the hex dump to decode
   * @return binary data
   */
  static std::vector<uint8_t> decode(const std::string &input);
};

#endif //OPENTELEMETRY_INCLUDE_HEX_H_
