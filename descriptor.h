#pragma once

#include <tusb.h>

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

class UsbDevice {
 public:
  UsbDevice();

  void SetVendorId(uint16_t id) { device_.idVendor = id; };
  void SetProductId(uint16_t id) { device_.idProduct = id; }
  void SetDeviceBcd(uint16_t bcd) { device_.bcdDevice = bcd; }

  void SetManufacturer(std::string_view str);
  void SetProduct(std::string_view str);
  void SetSerialNumber(std::string_view str);

  void AddCdc(std::string_view name);
  void AddMsc(std::string_view name);

  std::vector<uint8_t> DeviceDescriptor();
  std::vector<uint8_t> ConfigurationDescriptor();
  std::vector<uint16_t> StringDescriptor(uint8_t index);

  // Register this device with the TinyUSB stack.
  void Install();

 private:
  struct Interface {
    uint8_t interface_number;
    uint8_t endpoint_out;
    uint8_t endpoint_in;
  };

  Interface AddInterface();
  void ConfigurationAppend(std::initializer_list<uint8_t> interface_desc);
  uint8_t AddString(std::string_view str);

  tusb_desc_device_t device_;
  uint8_t interface_count_ = 0;
  std::vector<uint8_t> config_descriptor_tail_;
  std::vector<std::string> strings_;
};
