#include "descriptor.h"

#include <algorithm>
#include <iterator>

namespace usb {

Interface Configuration::AddInterface() {
  const uint8_t interface_number = interface_count_++;
  // Endpoint 0 is reserved.
  const uint8_t out = interface_number + 1;
  const uint8_t in = 0x80 | out;
  return {interface_number, out, in};
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

void AddCdc(Configuration& config, Strings& strings, std::string_view name) {
  const uint8_t string_index = strings.Add(name);
  const Interface control = config.AddInterface();
  const Interface data = config.AddInterface();
  // Interface number, string index, EP notification address and size, EP data
  // address (out, in) and size.
  config.Append({TUD_CDC_DESCRIPTOR(control.number, string_index, control.in, 8,
                                    data.out, data.in, 64)});
}

void AddMsc(Configuration& config, Strings& strings, std::string_view name) {
  const uint8_t string_index = strings.Add(name);
  const Interface data = config.AddInterface();
  // Interface number, string index, EP Out & EP In address, EP size
  config.Append(
      {TUD_MSC_DESCRIPTOR(data.number, string_index, data.out, data.in, 64)});
}

}  // namespace usb
