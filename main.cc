#include <bsp/board.h>
#include <hardware/timer.h>
#include <hardware/uart.h>
#include <pico/bootrom.h>
#include <pico/stdio/driver.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <tusb.h>

#include <array>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

#include "descriptor.h"
#include "fs.h"

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

struct Device {
  std::string name;
  std::optional<char> (*read)();
  void (*write)(char c);
};

std::array<Device, 2> devices{{
    {
        .name = "pc",
        .read = []() -> std::optional<char> {
          if (tud_cdc_n_available(1) > 0) {
            return tud_cdc_n_read_char(1);
          }
          return std::nullopt;
        },
        .write =
            [](char c) {
              tud_cdc_n_write_char(1, c);
              tud_cdc_n_write_flush(1);
            },
    },
    {
        .name = "motor",
        .read = []() -> std::optional<char> {
          if (uart_is_readable(uart0)) {
            return uart_getc(uart0);
          }
          return std::nullopt;
        },
        .write = [](char c) { uart_putc(uart0, c); },
    },
}};

Device& partner(const Device& device) {
  if (&device == &devices[0]) {
    return devices[1];
  }
  return devices[0];
}

int write_index = 0;

void transfer() {
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
  UsbDevice usb;
  usb.SetVendorId(0xCAFE);
  usb.SetProductId(0xB0BA);
  usb.SetDeviceBcd(0x1234);
  usb.SetManufacturer("DIY");
  usb.SetProduct("RS232 Bridge");
  usb.SetSerialNumber("123456");

  usb.AddCdc("Debug Console");
  usb.AddCdc("RS232 Data");
  usb.AddMsc("RS232 Storage");

  usb.Install();

  stdio_usb_init();
  uart_init(uart0, 38'400);
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);

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
  std::optional<FileSystem> fs;
  while (true) {
    if (!fs.has_value() && (time_us_64() - start_us > 5'000'000)) {
      fs.emplace();
    }
    tud_task();
    transfer();
  }
}
