#include "fs.h"

// clang-format off
#include <ff.h>
#include <diskio.h>
// clang-format on
#include <hardware/flash.h>
#include <tusb.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string_view>

namespace {
std::string ToHex(std::integral auto value) {
  std::stringstream ss;
  ss << std::setw(2) << std::setfill('0') << std::hex << "0x"
     << static_cast<int>(value);
  return ss.str();
}

constexpr int kSectorCount = 16;
constexpr int kSectorSize = 512;

std::array<std::byte, kSectorCount * kSectorSize> disk;
}  // namespace

///////////////////////////
// TinyUSB MSC callbacks //
///////////////////////////

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
  auto set = [&](uint8_t* dest, std::string_view str) {
    for (char c : str) {
      *(dest++) = c;
    }
    *(dest++) = 0;
  };

  set(vendor_id, "DIY");
  set(product_id, "RS232 Data");
  set(product_rev, "1.0");
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) { return false; }

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count,
                         uint16_t* block_size) {
  *block_count = 16;
  *block_size = kSectorSize;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject) {
  std::cout << "MSC Start Stop Unit command: start=" << start << " "
            << "load_eject=" << load_eject << std::endl;
  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void* buffer_start, uint32_t count) {
  auto buffer = std::span(static_cast<uint8_t*>(buffer_start), count);
  std::ranges::fill(buffer, 0);
  return count;
}

bool tud_msc_is_writeable_cb(uint8_t lun) { return false; }

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t* buffer, uint32_t count) {
  return -1;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                        uint16_t count) {
  std::cout << "MSC SCSI command: " << ToHex(scsi_cmd[0]) << std::endl;
  switch (scsi_cmd[0]) {
    case 0x1E:
      return 0;
    default:
      std::cout << "Unsupported SCSI command." << std::endl;
      return -1;
  }
}

/////////////////////
// FatFS callbacks //
/////////////////////

DSTATUS disk_initialize(BYTE drive) { return 0; }

DSTATUS disk_status(BYTE drive) { return 0; }

DRESULT disk_ioctl(BYTE drive, BYTE command, void* buffer) {
  std::cout << "disk_ioctl: command=" << ToHex(command);
  switch (command) {
    case CTRL_SYNC:
      return RES_OK;
    case GET_SECTOR_COUNT: {
      *reinterpret_cast<LBA_t*>(buffer) = kSectorCount;
      return RES_OK;
    }
    case GET_BLOCK_SIZE: {
      *reinterpret_cast<DWORD*>(buffer) = 1;
      return RES_OK;
    }
    default:
      std::cout << "Unsupported ioctl command." << std::endl;
      return RES_PARERR;
  }
  return RES_OK;
}

DRESULT disk_read(BYTE drive, BYTE* buffer, LBA_t sector, UINT sector_count) {
  if (sector < 0 || sector >= kSectorCount) {
    std::cout << "disk_read invalid sector number: " << sector << std::endl;
    return RES_PARERR;
  }
  std::memcpy(buffer, disk.data() + sector * kSectorSize,
              sector_count * kSectorSize);
  return RES_OK;
}

DRESULT disk_write(BYTE drive, const BYTE* buffer, LBA_t sector,
                   UINT sector_count) {
  if (sector < 0 || sector >= kSectorCount) {
    std::cout << "disk_write invalid sector number: " << sector << std::endl;
    return RES_PARERR;
  }
  std::memcpy(disk.data() + sector * kSectorSize, buffer,
              sector_count * kSectorSize);
  return RES_OK;
}

DWORD get_fattime() {
  std::bitset<32> time;
  // Set month to January (1).
  time[21] = 1;
  // Set day to 1
  time[16] = 1;
  return time.to_ulong();
}

/////////
// API //
/////////

namespace {
FATFS fs;
}  // namespace

void FsInit() {
  std::array<BYTE, kSectorSize> work_area;
  f_mkfs("", nullptr, work_area.data(), work_area.size());
}
