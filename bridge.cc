#include "bridge.h"

#include <fmt/core.h>

#include <charconv>
#include <iomanip>
#include <ios>
#include <iostream>
#include <regex>
#include <sstream>

namespace {
int ParseNonce(std::string_view str) {
  if (str.empty()) {
    return -1;
  }
  int value;
  const std::from_chars_result result =
      std::from_chars(str.begin(), str.end(), value);
  if (result.ptr != str.end()) {
    throw std::system_error(std::make_error_code(result.ec),
                            "Corrupted nonce file contents.");
  }
  return value;
}
}  // namespace

Bridge::Bridge(CdcDevice& usb, uart_inst_t& uart, FileSystem& fs)
    : usb_(usb), uart_(uart), fs_(fs) {
  // Use a new nonce for each run, keeping track of the previous nonce in a
  // file.
  File nonce_file = fs_.OpenFile(
      "/nonce.txt", {.read = true, .write = true, .open_always = true});
  const int previous_nonce = ParseNonce(nonce_file.ReadAll());
  const int nonce = previous_nonce + 1;
  std::cout << "Nonce: " << nonce << std::endl;
  nonce_file.Seek(0);
  nonce_file.Write(std::to_string(nonce));
}

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
    std::cout << fmt::format("{} {}: {:#04x}", write_index_, Name(device), c)
              << std::endl;
    ++write_index_;

    Write(Partner(device), c);
  }
}
