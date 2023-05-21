#include "descriptor.h"

#include <algorithm>
#include <iterator>

namespace usb {

EndpointPair Configuration::AddInterface() {
  const uint8_t out = interface_count_++;
  const uint8_t in = 0x80 | out;
  return {out, in};
}

void Configuration::Append(std::initializer_list<uint8_t> interface_desc) {
  std::ranges::copy(interface_desc, std::back_inserter(descriptor_tail_));
}

std::vector<uint8_t> Configuration::Descriptor() {
  const int config_length = TUD_CONFIG_DESC_LEN + descriptor_tail_.size();
  // Config number, interface count, string index, total length, attribute,
  // power in mA
  std::vector<uint8_t> descriptor = {
      TUD_CONFIG_DESCRIPTOR(1, interface_count_, 0, config_length, 0, 100)};
  std::ranges::copy(descriptor_tail_, std::back_inserter(descriptor));
  return descriptor;
}

uint8_t Strings::Add(std::string_view str) {
  strings_.emplace_back(str);
  return strings_.size() - 1;
}

std::array<uint16_t, 32> Strings::Descriptor(uint8_t index) {
  if (index >= strings_.size()) {
    return {};
  }
  const std::string& str = strings_[index];
  std::array<uint16_t, 32> descriptor;
  // 16-bit header. First byte is the byte count (including the header). Second
  // byte is the string type.
  const uint16_t length = 2 * str.size() + 2;
  const uint16_t string_type = TUSB_DESC_STRING;
  descriptor[0] = (string_type << 8) | length;
  // Widen each 8-bit value to 16-bit.
  uint16_t* c16 = &descriptor[1];
  for (char c : str) {
    *(c16++) = c;
  }
  return descriptor;
}

}  // namespace usb
