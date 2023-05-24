#include <cxxabi.h>
#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <hardware/uart.h>
#include <pico/stdlib.h>

#include <iostream>

#include "bridge.h"
#include "fs.h"
#include "usb_device.h"

int main() {
  std::set_terminate(__gnu_cxx::__verbose_terminate_handler);

  FlashDisk disk(256);

  UsbDevice usb;
  usb.SetVendorId(0xCAFE);
  usb.SetProductId(0xB0BA);
  usb.SetDeviceBcd(0x1234);
  usb.SetManufacturer("DIY");
  usb.SetProduct("RS232 Bridge");
  usb.SetSerialNumber("123456");

  CdcDevice& stdio_cdc = usb.AddCdc("Debug Console");
  CdcDevice& data_cdc = usb.AddCdc("RS232 Data");
  MscDevice& msc = usb.AddMsc("RS232 Storage", disk);
  msc.SetVendorId("DIY");
  msc.SetProductId("RS232 Storage");
  msc.SetProductRev("1.0");

  usb.Install();
  stdio_usb_init();

  while (!stdio_cdc.Connected()) {
    sleep_ms(10);
  }
  std::cout << "====\nStartup" << std::endl;
  FileSystem fs(disk);
  fs.Install();
  msc.SetReady();

  uart_init(uart0, 38'400);
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);

  Bridge bridge(data_cdc, *uart0, fs);

  while (true) {
    usb.Task();
    bridge.Task();
  }
}
