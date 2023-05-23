#pragma once

#include <hardware/uart.h>

#include <optional>
#include <string_view>

#include "cdc_device.h"

class Bridge {
 public:
  Bridge(CdcDevice& usb, uart_inst_t& uart) : usb_(usb), uart_(uart) {}

  void Task();

 private:
  enum class Device {
    kUsb,
    kUart,
  };

  std::string_view Name(Device device);
  std::optional<char> Read(Device device);
  void Write(Device device, char c);
  Device Partner(Device device);

  CdcDevice& usb_;
  uart_inst_t& uart_;

  int write_index_ = 0;
};
