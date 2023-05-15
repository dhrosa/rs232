#include <hardware/timer.h>
#include <hardware/uart.h>
#include <pico/bootrom.h>
#include <pico/stdio/driver.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include <array>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "bsp/board.h"
#include "tusb.h"

struct CdcEndpoints {
  uint8_t control;
  uint8_t out;
  uint8_t in;
};

constexpr std::array<CdcEndpoints, 2> endpoints = {{
    {.control = 0x81, .out = 0x02, .in = 0x82},
    {.control = 0x83, .out = 0x04, .in = 0x84},
}};

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
  // Each CDC interface needs two interfaces; control and data.
  constexpr int cdc_count = endpoints.size();
  constexpr int interface_count = cdc_count * 2;
  constexpr uint16_t config_length =
      TUD_CONFIG_DESC_LEN + cdc_count * TUD_CDC_DESC_LEN;

  static const uint8_t configuration[] = {
      // Config number, interface count, string index, total length, attribute,
      // power in mA
      TUD_CONFIG_DESCRIPTOR(1, interface_count, 0, config_length, 0x00, 100),
      // Interface number, string index, EP notification address and size, EP
      // data
      // address (out, in) and size.
      TUD_CDC_DESCRIPTOR(0, 4, endpoints[0].control, 8, endpoints[0].out,
                         endpoints[0].in, 64),
      TUD_CDC_DESCRIPTOR(2, 4, endpoints[1].control, 8, endpoints[1].out,
                         endpoints[1].in, 64),
  };
  return configuration;
}

std::array<uint16_t, 32> DescriptorString(std::span<const uint16_t> str) {
  std::array<uint16_t, 32> descriptor;
  for (int i = 0; i < str.size(); ++i) {
    descriptor[i + 1] = str[i];
  }
  // 16-bit header. First byte is the byte count (including the header). Second
  // byte is the string type.
  const uint16_t length = 2 * str.size() + 2;
  const uint16_t string_type = TUSB_DESC_STRING;
  descriptor[0] = (string_type << 8) | length;
  return descriptor;
}

std::array<uint16_t, 32> DescriptorString(std::string_view str) {
  std::vector<uint16_t> str16;
  for (char c : str) {
    str16.push_back(c);
  }
  return DescriptorString(str16);
}

extern "C" const uint16_t* tud_descriptor_string_cb(uint8_t index,
                                                    uint16_t langid) {
  static std::array<uint16_t, 32> storage;
  switch (index) {
    // Language
    case 0: {
      const uint16_t language_eng[] = {0x0409};
      storage = DescriptorString(language_eng);
      break;
    }
    case 1:
      // Manufacturer
      storage = DescriptorString("DIY");
      break;
    case 2:
      // Product
      storage = DescriptorString("RS232 bridge");
      break;
    case 3:
      // Serial
      storage = DescriptorString("123456");
      break;
    case 4:
      // CDC interface
      storage = DescriptorString("DIY CDC");
      break;
    default:
      return nullptr;
  }
  return storage.data();
}

extern "C" void tud_cdc_line_coding_cb(uint8_t itf,
                                       const cdc_line_coding_t* line_coding) {
  if (line_coding->bit_rate == 1200) {
    std::cout << "Resetting to bootloader." << std::endl;
    reset_usb_boot(0, 0);
  }
}

void cdc_task() {
  for (int i = 0; i < 2; ++i) {
    if (tud_cdc_n_available(i) == 0) {
      continue;
    }
    const int value = tud_cdc_n_read_char(i);
    std::cout << i << ": " << value << std::endl;
  }
}

int main() {
  tud_init(0);
  stdio_usb_init();
  while (true) {
    tud_task();
  }
}
