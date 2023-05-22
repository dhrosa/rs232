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

const uint8_t* tud_descriptor_device_cb() {
  static const tusb_desc_device_t descriptor = {
      .bLength = sizeof(tusb_desc_device_t),
      .bDescriptorType = TUSB_DESC_DEVICE,
      // USB1.1
      .bcdUSB = 0x0110,
      // Use Interface Association Descriptor (IAD) for CDC As required by USB
      // Specs IAD's subclass must be common class (2) and protocol must be IAD
      // (1)
      .bDeviceClass = TUSB_CLASS_MISC,
      .bDeviceSubClass = MISC_SUBCLASS_COMMON,
      .bDeviceProtocol = MISC_PROTOCOL_IAD,
      .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
      .idVendor = 0xCAFE,
      .idProduct = 0xB0BA,
      .bcdDevice = 0x0100,
      .iManufacturer = 1,
      .iProduct = 2,
      .iSerialNumber = 3,
      .bNumConfigurations = 0x01,
  };

  return reinterpret_cast<const uint8_t*>(&descriptor);
}

usb::Configuration usb_config;
usb::Strings usb_strings;

const uint8_t* tud_descriptor_configuration_cb(uint8_t index) {
  static std::vector<uint8_t> configuration = usb_config.Descriptor();
  return configuration.data();
}

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  if (index >= usb_strings.Size()) {
    return nullptr;
  }
  static std::array<uint16_t, 32> descriptor;
  descriptor = usb_strings.Descriptor(index);
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
  // Language (English)
  usb_strings.Add((const char[]){0x09, 0x04});
  // Manufacturer
  usb_strings.Add("DIY");
  // Product
  usb_strings.Add("RS232 Bridge");
  // Serial
  usb_strings.Add("123456");

  usb::AddCdc(usb_config, usb_strings, "Debug Console");
  usb::AddCdc(usb_config, usb_strings, "RS232 Data");
  usb::AddMsc(usb_config, usb_strings, "RS232 Storage");

  tud_init(0);
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
