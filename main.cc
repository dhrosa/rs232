#include <hardware/timer.h>
#include <hardware/uart.h>
#include <pico/bootrom.h>
#include <pico/stdio/driver.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include <array>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

#include "bsp/board.h"
#include "tusb.h"

extern "C" const uint8_t* tud_descriptor_device_cb() {
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
      .idProduct = 0xBEEF,
      .bcdDevice = 0x0100,
      .iManufacturer = 1,
      .iProduct = 2,
      .iSerialNumber = 3,
      .bNumConfigurations = 0x01,
  };

  return reinterpret_cast<const uint8_t*>(&descriptor);
}

extern "C" const uint8_t* tud_descriptor_configuration_cb(uint8_t index) {
  static std::vector<uint8_t> configuration;
  auto append = [&](std::initializer_list<uint8_t> descriptor) {
    configuration.insert(configuration.end(), descriptor);
  };

  // Each CDC interface needs two interfaces; control (IN) and data (OUT and
  // IN).
  const std::array<std::array<uint8_t, 3>, 2> cdc_endpoints{{
      {0x81, 0x02, 0x82},
      {0x83, 0x04, 0x84},
  }};
  constexpr uint16_t config_length =
      TUD_CONFIG_DESC_LEN + cdc_endpoints.size() * TUD_CDC_DESC_LEN;

  // Config number, interface count, string index, total length, attribute,
  // power in mA
  append({TUD_CONFIG_DESCRIPTOR(1, 2 * cdc_endpoints.size(), 0, config_length,
                                0x00, 100)});
  uint8_t interface = 0;
  for (const auto [ep_control, ep_out, ep_in] : cdc_endpoints) {
    // Interface number, string index, EP notification address and size, EP
    // data
    // address (out, in) and size.
    append(
        {TUD_CDC_DESCRIPTOR(interface, 4, ep_control, 8, ep_out, ep_in, 64)});
    interface += 2;
  }
  return configuration.data();
}

extern "C" const uint16_t* tud_descriptor_string_cb(uint8_t index,
                                                    uint16_t langid) {
  const std::array<std::span<const char>, 5> strings = {
      // Language (English)
      (const char[]){0x09, 0x04},
      // Manufacturer
      "DIY",
      // Product
      "RS232 bridge",
      // Serial
      "123456",
      // CDC interface
      "DIY CDC",
  };
  if (index >= strings.size()) {
    return nullptr;
  }
  const auto str = strings[index];
  static std::array<uint16_t, 32> descriptor;
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
  return descriptor.data();
}

extern "C" void tud_cdc_line_coding_cb(uint8_t itf,
                                       const cdc_line_coding_t* coding) {
  std::cout << "CDC" << itf
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

extern "C" void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  std::cout << "CDC" << itf << " line state change: dtr=" << dtr
            << " rts=" << rts << std::endl;
}

extern "C" void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms) {
  std::cout << "CDC" << itf << " break request: duration_ms=" << duration_ms
            << std::endl;
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
        .write = [](char c) { tud_cdc_n_write_char(1, c); },
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
  while (true) {
    tud_task();
    transfer();
  }
}
