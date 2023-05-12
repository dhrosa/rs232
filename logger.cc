#include <pico/stdlib.h>
#include <pico/sync.h>
#include <pico/time.h>

#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <utility>

constexpr int baudrate = 115'200;

uart_inst_t* const pc = uart0;
uart_inst_t* const motor = uart1;

const unsigned pin_rx0 = 1;
const unsigned pin_rx1 = 25;

uart_inst_t* readable_uart() {
  for (auto* uart : {pc, motor}) {
    if (uart_is_readable(uart)) {
      return uart;
    }
  }
  return nullptr;
}

std::string_view uart_name(uart_inst_t* uart) {
  static constexpr std::string_view pc_name = "pc";
  static constexpr std::string_view motor_name = "motor";
  if (uart == pc) return pc_name;
  return motor_name;
}

int main() {
  stdio_usb_init();
  std::cout << "Startup delay" << std::endl;
  sleep_ms(3000);
  std::cout << "Startup delay complete" << std::endl;

  for (auto* uart : {pc, motor}) {
    std::cout << "baudrate: " << uart_init(uart, baudrate);
    uart_set_hw_flow(uart, false, false);
    uart_set_format(uart, 8, 1, UART_PARITY_NONE);
  }
  gpio_set_function(pin_rx0, GPIO_FUNC_UART);
  gpio_set_function(pin_rx1, GPIO_FUNC_UART);

  while (true) {
    // uart_inst_t* uart = readable_uart();
    // if (uart == nullptr) {
    //   sleep_ms(1);
    //   continue;
    // }
    // //std::cout << uart_name(uart) << ":" << std::hex << std::setfill('0');
    // while (uart_is_readable(uart)) {
    //   const char value = uart_getc(uart);
    //   std::cout << " " << std::setw(2) << static_cast<unsigned int>(value);
    // }
    // std::cout << std::endl;
    const char c = uart_getc(uart0);
    std::cout << c;
    std::cout.flush();
  }
  return 0;
}
