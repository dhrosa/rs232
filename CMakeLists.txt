cmake_minimum_required(VERSION 3.24)
set(CMAKE_CXX_STANDARD 23)

set(FAMILY rp2040)
set(BOARD pico_sdk)

set(PICO_BOARD adafruit_feather_rp2040)
set(PICO_SDK_FETCH_FROM_GIT on)

include(pico_sdk_import.cmake)

project(rs232 LANGUAGES C CXX)
pico_sdk_init()

add_executable(rs232 main.cc)
target_include_directories(rs232 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(
  rs232
  PUBLIC
  pico_stdlib
  hardware_uart
  pico_time
  pico_bootsel_via_double_reset
  tinyusb_board
  tinyusb_device
)
#target_compile_definitions(rs232 PUBLIC CFG_TUSB_DEBUG=2)
pico_enable_stdio_usb(rs232 0)
pico_enable_stdio_uart(rs232 1)

pico_add_extra_outputs(rs232)




add_executable(logger logger.cc)
target_link_libraries(logger PUBLIC pico_stdlib hardware_uart pico_bootsel_via_double_reset)
pico_enable_stdio_usb(logger 1)
pico_enable_stdio_uart(logger 0)
pico_add_extra_outputs(logger)
