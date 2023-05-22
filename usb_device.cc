#include "usb_device.h"

#include <pico/bootrom.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <span>

namespace {
UsbDevice* g_device;
};  // namespace

///////////////////////
// TinyUSB callbacks //
///////////////////////

const uint8_t* tud_descriptor_device_cb() {
  static std::vector<uint8_t> descriptor;
  descriptor = g_device->DeviceDescriptor();
  return descriptor.data();
}

const uint8_t* tud_descriptor_configuration_cb(uint8_t index) {
  static std::vector<uint8_t> descriptor;
  descriptor = g_device->ConfigurationDescriptor();
  return descriptor.data();
}

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  static std::vector<uint16_t> descriptor;
  descriptor = g_device->StringDescriptor(index);
  if (descriptor.empty()) {
    return nullptr;
  }
  return descriptor.data();
}

void tud_cdc_line_coding_cb(uint8_t itf, const cdc_line_coding_t* coding) {
  std::cout << "CDC" << static_cast<int>(itf)
            << " line coding change: bit_rate=" << coding->bit_rate
            << " stop_bits=" << static_cast<int>(coding->stop_bits)
            << " parity=" << static_cast<int>(coding->parity)
            << " data_bits=" << static_cast<int>(coding->data_bits)
            << std::endl;
  if (coding->bit_rate == 1200) {
    std::cout << "Resetting to bootloader." << std::endl;
    reset_usb_boot(0, 0);
  }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  std::cout << "CDC" << static_cast<int>(itf)
            << " line state change: dtr=" << dtr << " rts=" << rts << std::endl;
}

void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms) {
  std::cout << "CDC" << static_cast<int>(itf)
            << " break request: duration_ms=" << duration_ms << std::endl;
}

UsbDevice::UsbDevice()
    : device_{
          .bLength = sizeof(tusb_desc_device_t),
          .bDescriptorType = TUSB_DESC_DEVICE,
          .bDeviceClass = TUSB_CLASS_MISC,
          .bDeviceSubClass = MISC_SUBCLASS_COMMON,
          .bDeviceProtocol = MISC_PROTOCOL_IAD,
          .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
          .bNumConfigurations = 1,
      } {
  // Language (English)
  AddString((const char[]){0x09, 0x04});
}

void UsbDevice::SetManufacturer(std::string_view str) {
  device_.iManufacturer = AddString(str);
}

void UsbDevice::SetProduct(std::string_view str) {
  device_.iProduct = AddString(str);
}

void UsbDevice::SetSerialNumber(std::string_view str) {
  device_.iSerialNumber = AddString(str);
}

UsbDevice::Interface UsbDevice::AddInterface() {
  const uint8_t interface_number = interface_count_++;
  // Endpoint 0 is reserved.
  const uint8_t out = interface_number + 1;
  const uint8_t in = 0x80 | out;
  return {interface_number, out, in};
}

void UsbDevice::ConfigurationAppend(
    std::initializer_list<uint8_t> interface_desc) {
  std::ranges::copy(interface_desc,
                    std::back_inserter(config_descriptor_tail_));
}

std::vector<uint8_t> UsbDevice::DeviceDescriptor() {
  std::vector<uint8_t> descriptor;
  std::ranges::copy(
      std::span(reinterpret_cast<const uint8_t*>(&device_), sizeof(device_)),
      std::back_inserter(descriptor));
  return descriptor;
}

std::vector<uint8_t> UsbDevice::ConfigurationDescriptor() {
  const int config_length =
      TUD_CONFIG_DESC_LEN + config_descriptor_tail_.size();
  // Config number, interface count, string index, total length, attribute,
  // power in mA
  std::vector<uint8_t> descriptor = {
      TUD_CONFIG_DESCRIPTOR(1, interface_count_, 0, config_length, 0, 100)};
  std::ranges::copy(config_descriptor_tail_, std::back_inserter(descriptor));
  return descriptor;
}

uint8_t UsbDevice::AddString(std::string_view str) {
  strings_.emplace_back(str);
  return strings_.size() - 1;
}

std::vector<uint16_t> UsbDevice::StringDescriptor(uint8_t index) {
  if (index >= strings_.size()) {
    return {};
  }
  const std::string& str = strings_[index];
  std::vector<uint16_t> descriptor;
  // 16-bit header. First byte is the byte count (including the header). Second
  // byte is the string type.
  const uint16_t length = 2 * str.size() + 2;
  const uint16_t string_type = TUSB_DESC_STRING;
  descriptor.push_back((string_type << 8) | length);
  // Widen each 8-bit value to 16-bit.
  for (char c : str) {
    descriptor.push_back(c);
  }
  return descriptor;
}

CdcDevice& UsbDevice::AddCdc(std::string_view name) {
  const uint8_t string_index = AddString(name);
  const Interface control = AddInterface();
  const Interface data = AddInterface();
  // Interface number, string index, EP notification address and size, EP data
  // address (out, in) and size.
  ConfigurationAppend({TUD_CDC_DESCRIPTOR(
      control.interface_number, string_index, control.endpoint_in, 8,
      data.endpoint_out, data.endpoint_in, 64)});

  return cdc_.emplace_back(cdc_.size());
}

void UsbDevice::AddMsc(std::string_view name) {
  const uint8_t string_index = AddString(name);
  const Interface data = AddInterface();
  // Interface number, string index, EP Out & EP In address, EP size
  ConfigurationAppend(
      {TUD_MSC_DESCRIPTOR(data.interface_number, string_index,
                          data.endpoint_out, data.endpoint_in, 64)});
}

void UsbDevice::Install() {
  g_device = this;
  tud_init(0);
}
