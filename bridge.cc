#include "bridge.h"

#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>

std::string_view Bridge::Name(Device device) {
  if (device == Device::kUsb) {
    return "USB";
  }
  return "UART";
}

std::optional<char> Bridge::Read(Device device) {
  if (device == Device::kUsb && usb_.ReadAvailable() > 0) {
    return usb_.ReadChar();
  }
  if (device == Device::kUart && uart_is_readable(&uart_)) {
    return uart_getc(&uart_);
  }
  return std::nullopt;
}

void Bridge::Write(Device device, char c) {
  if (device == Device::kUsb) {
    usb_.WriteChar(c);
    usb_.Flush();
    return;
  }
  uart_putc(&uart_, c);
}

Bridge::Device Bridge::Partner(Device device) {
  if (device == Device::kUsb) {
    return Device::kUart;
  }
  return Device::kUsb;
}

void Bridge::Task() {
  for (Device device : {Device::kUsb, Device::kUart}) {
    const std::optional<char> oc = Read(device);
    if (!oc) {
      continue;
    }
    const char c = *oc;

    // Log the transfer
    std::stringstream value_str;
    value_str << "0x" << std::hex << std::setw(2) << std::setfill('0')
              << std::uppercase << static_cast<int>(c);
    std::cout << write_index_ << " " << Name(device) << ": " << value_str.view()
              << std::endl;
    ++write_index_;

    Write(Partner(device), c);
  }
}
