#pragma once

#include <tusb.h>

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace usb {

struct EndpointPair {
  uint8_t out;
  uint8_t in;
};

class Configuration {
 public:
  EndpointPair AddInterface();

  void Append(std::initializer_list<uint8_t> interface_desc);

  std::vector<uint8_t> Descriptor();

 private:
  uint8_t interface_count_ = 1;

  std::vector<uint8_t> descriptor_tail_;
};

class Strings {
 public:
  // Returns the index of the added string.
  uint8_t Add(std::string_view str);

  std::array<uint16_t, 32> Descriptor(uint8_t index);

 private:
  std::vector<std::string> strings_;
};

}  // namespace usb
