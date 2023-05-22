#include <bsp/board.h>
#include <hardware/timer.h>
#include <hardware/uart.h>
#include <pico/stdio/driver.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <tusb.h>

#include <array>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

#include "fs.h"
#include "usb_device.h"

struct Device {
  std::string name;
  std::move_only_function<std::optional<char>()> read;
  std::move_only_function<void(char c)> write;
};

std::array<Device, 2> devices;

Device& partner(const Device& device) {
  if (&device == &devices[0]) {
    return devices[1];
  }
  return devices[0];
}

void transfer() {
  static int write_index = 0;
  for (Device& device : devices) {
    const std::optional<char> oc = device.read();
    if (!oc) {
      continue;
    }
    const char c = *oc;

    // Log the transfer
    std::stringstream value_str;
    value_str << "0x" << std::hex << std::setw(2) << std::setfill('0')
              << std::uppercase << static_cast<int>(c);
    std::cout << write_index << " " << device.name << ": " << value_str.view()
              << std::endl;
    ++write_index;

    partner(device).write(c);
  }
}

int main() {
  std::set_terminate([]() {
    std::cout << "Unhandled exception: " << std::endl;
    try {
      std::rethrow_exception(std::current_exception());
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
    while (true) {
      tight_loop_contents();
    }
  });

  FlashDisk disk(256);

  UsbDevice usb;
  usb.SetVendorId(0xCAFE);
  usb.SetProductId(0xB0BA);
  usb.SetDeviceBcd(0x1234);
  usb.SetManufacturer("DIY");
  usb.SetProduct("RS232 Bridge");
  usb.SetSerialNumber("123456");

  usb.AddCdc("Debug Console");
  CdcDevice& data_cdc = usb.AddCdc("RS232 Data");
  MscDevice& msc = usb.AddMsc("RS232 Storage", disk);
  msc.SetVendorId("DIY");
  msc.SetProductId("RS232 Storage");
  msc.SetProductRev("1.0");

  usb.Install();

  stdio_usb_init();
  uart_init(uart0, 38'400);
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);

  devices[0] = {
      .name = "pc",
      .read = [&]() -> std::optional<char> {
        if (data_cdc.ReadAvailable() > 1) {
          return data_cdc.ReadChar();
        }
        return std::nullopt;
      },
      .write =
          [&](char c) {
            data_cdc.WriteChar(c);
            data_cdc.Flush();
          },
  };
  devices[1] = {
      .name = "motor",
      .read = []() -> std::optional<char> {
        if (uart_is_readable(uart0)) {
          return uart_getc(uart0);
        }
        return std::nullopt;
      },
      .write = [](char c) { uart_putc(uart0, c); },
  };

  FileSystem fs(disk);

  // Additionally update TinyUSB in the background in-case main() is busy with a
  // blocking operation.
  repeating_timer_t timer;
  add_repeating_timer_ms(
      1,
      [](repeating_timer_t*) {
        tud_task();
        return true;
      },
      nullptr, &timer);

  const auto start_us = time_us_64();
  bool fs_installed = false;
  while (true) {
    if (!fs_installed && (time_us_64() - start_us > 5'000'000)) {
      fs.Install();
      fs_installed = true;
      msc.SetReady();
    }
    tud_task();
    transfer();
  }
}
