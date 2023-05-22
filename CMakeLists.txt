cmake_minimum_required(VERSION 3.24)
set(CMAKE_CXX_STANDARD 23)

set(FAMILY rp2040)
set(BOARD pico_sdk)

set(PICO_BOARD adafruit_feather_rp2040)
set(PICO_SDK_FETCH_FROM_GIT on)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

include(pico_sdk_import.cmake)

project(rs232 LANGUAGES C CXX)
pico_sdk_init()

include(FetchContent)
FetchContent_Declare(fatfs_upstream
  URL http://elm-chan.org/fsw/ff/arc/ff15.zip
)
FetchContent_MakeAvailable(fatfs_upstream)
add_subdirectory(fatfs)

add_executable(rs232 main.cc fs.cc usb_device.cc)
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
  fatfs
)
pico_enable_stdio_usb(rs232 1)
pico_enable_stdio_uart(rs232 0)

pico_add_extra_outputs(rs232)
