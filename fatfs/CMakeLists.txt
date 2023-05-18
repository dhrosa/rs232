# Patches FATFS ffconf.h

add_library(fatfs)

foreach(f IN ITEMS ff.h ff.c diskio.h)
  add_custom_command(OUTPUT ${f}
    COMMAND ${CMAKE_COMMAND} -E copy "${fatfs_upstream_SOURCE_DIR}/source/${f}" "${CMAKE_CURRENT_BINARY_DIR}/${f}")
endforeach()

add_custom_command(OUTPUT ffconf.h
  COMMAND sed "s/#define FF_USE_MKFS.*/#define FF_USE_MKFS 1/g" "${fatfs_upstream_SOURCE_DIR}/source/ffconf.h" > "${CMAKE_CURRENT_BINARY_DIR}/ffconf.h")
add_custom_target(fatfs_headers DEPENDS ffconf.h ff.h diskio.h)

target_include_directories(fatfs PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(fatfs fatfs_headers)
target_sources(fatfs PRIVATE ff.c)
