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
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
std::string ToHex(auto value) {
  std::stringstream ss;
  ss << std::setw(2) << std::setfill('0') << std::hex << "0x"
     << static_cast<int>(value);
  return ss.str();
}

void LogSpan(std::ostream& s, std::span<const uint8_t> bytes) {
  auto flags = s.flags();
  s << std::hex << std::setfill('0');
  for (int i = 0; i < bytes.size(); ++i) {
    if (i % 32 == 0) {
      s << "\n" << std::setw(4) << i << ": ";
    }
    if (i % 2 == 0) {
      s << " ";
    }
    s << std::setw(2) << static_cast<int>(bytes[i]);
  }
  s << std::endl;
  s.flags(flags);
}

constexpr int kSectorCount = 256;
constexpr int kSectorSize = 4096;

const std::span<const uint8_t> flash(reinterpret_cast<const uint8_t*>(XIP_BASE),
                                     PICO_FLASH_SIZE_BYTES);

const std::span<const uint8_t> disk = flash.last(kSectorCount * kSectorSize);

struct InterruptBlocker {
  InterruptBlocker() { saved = save_and_disable_interrupts(); }

  ~InterruptBlocker() { restore_interrupts(saved); }

  uint32_t saved;
};

void FlashWriteSector(const uint8_t* dest, const uint8_t* source) {
  const uint32_t offset = dest - flash.data();
  InterruptBlocker blocker;
  flash_range_erase(offset, kSectorSize);
  flash_range_program(offset, source, kSectorSize);
}

bool fs_initialized = false;
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

bool tud_msc_test_unit_ready_cb(uint8_t lun) { return fs_initialized; }

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count,
                         uint16_t* block_size) {
  *block_count = kSectorCount;
  *block_size = kSectorSize;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject) {
  std::cout << "MSC Start Stop Unit command: start=" << start << " "
            << "load_eject=" << load_eject << std::endl;
  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void* buffer, uint32_t count) {
  const std::size_t start = lba * kSectorSize + offset;
  const std::size_t end = start + count;
  if (start >= disk.size() || end >= disk.size()) {
    std::cout << "MSC read out of range read: start=" << start << " end=" << end
              << std::endl;
    return -1;
  }
  std::memcpy(buffer, disk.data() + start, count);
  return count;
}

bool tud_msc_is_writeable_cb(uint8_t lun) { return true; }

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t* buffer, uint32_t count) {
  const std::size_t start = lba * kSectorSize + offset;
  const std::size_t end = start + count;
  if (start >= disk.size() || end >= disk.size()) {
    std::cout << "MSC read out of range write: start=" << start
              << " end=" << end << std::endl;
    return -1;
  }
  return -1;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                        uint16_t count) {
  const uint8_t op = scsi_cmd[0];
  switch (op) {
    case 0x1E:
      return 0;
    default:
      std::cout << "Unsupported SCSI operation: 0x" << ToHex(op) << std::endl;
      return -1;
  }
}

/////////////////////
// FatFS callbacks //
/////////////////////

DSTATUS disk_initialize(BYTE drive) { return 0; }

DSTATUS disk_status(BYTE drive) { return 0; }

DRESULT disk_ioctl(BYTE drive, BYTE command, void* buffer) {
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
      std::cout << "Unsupported disk_ioctl command:" << ToHex(command)
                << std::endl;
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

DRESULT disk_write(BYTE drive, const BYTE* buffer, LBA_t start_sector,
                   UINT sector_count) {
  if (start_sector < 0 || start_sector >= kSectorCount) {
    std::cout << "disk_write invalid sector number: " << start_sector
              << std::endl;
    return RES_PARERR;
  }

  const std::uint8_t* dest = disk.data() + start_sector * kSectorSize;
  const std::uint8_t* source = buffer;
  for (int i = start_sector; i < start_sector + sector_count; ++i) {
    FlashWriteSector(dest, source);
    dest += kSectorSize;
    source += kSectorSize;
  }

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

PARTITION VolToPart[] = {
    {.pd = 0, .pt = 1},
};

/////////
// API //
/////////

namespace {
void ThrowIfError(FRESULT result) {
  if (result == FR_OK) {
    return;
  }
  std::stringstream what;
  what << "FAT FS error: " << static_cast<int>(result);
  throw std::filesystem::filesystem_error(
      what.str(), std::make_error_code(std::errc::io_error));
}
}  // namespace

FileSystem::FileSystem() {
  std::cout << "FAT file system initialization start." << std::endl;
  const LBA_t partition_sizes[] = {kSectorCount - 5};
  std::array<BYTE, kSectorSize> work_area;
  ThrowIfError(f_fdisk(0, partition_sizes, work_area.data()));
  ThrowIfError(f_mkfs("0:", nullptr, work_area.data(), work_area.size()));
  ThrowIfError(f_mount(&fs_, "", 1));
  std::cout << "FAT file system initialization complete." << std::endl;
  fs_initialized = true;
}

File File::Open(std::filesystem::path path, const OpenFlags& flags) {
  BYTE mode;
  auto add_flag = [&](bool enable, BYTE flag) {
    if (enable) {
      mode |= flag;
    }
  };

  add_flag(flags.read, FA_READ);
  add_flag(flags.write, FA_WRITE);
  add_flag(flags.open_existing, FA_OPEN_EXISTING);
  add_flag(flags.create_new, FA_CREATE_NEW);
  add_flag(flags.create_always, FA_CREATE_ALWAYS);
  add_flag(flags.open_always, FA_OPEN_ALWAYS);
  add_flag(flags.open_append, FA_OPEN_APPEND);

  File file;
  file.fat_file_ = std::make_unique<FIL>();
  ThrowIfError(f_open(file.fat_file_.get(), path.c_str(), mode));
  return file;
}

File::~File() {
  if (fat_file_ == nullptr) {
    return;
  }
  try {
    Close();
  } catch (const std::filesystem::filesystem_error& e) {
    std::cout << "Error while destructing file: " << e.what() << std::endl;
  }
}

void File::Close() {
  ThrowIfError(f_close(std::exchange(fat_file_, nullptr).get()));
}

int File::Tell() { return f_tell(fat_file_.get()); }

void File::Seek(int location) {
  ThrowIfError(f_lseek(fat_file_.get(), location));
}

std::span<std::byte> File::Read(std::span<std::byte> buffer) {
  UINT bytes_read;
  ThrowIfError(
      f_read(fat_file_.get(), buffer.data(), buffer.size(), &bytes_read));
  return buffer.subspan(bytes_read);
}

int File::Write(std::span<const std::byte> buffer) {
  UINT bytes_written;
  ThrowIfError(
      f_write(fat_file_.get(), buffer.data(), buffer.size(), &bytes_written));
  return bytes_written;
}

void File::Sync() { ThrowIfError(f_sync(fat_file_.get())); }

Directory Directory::Open(std::filesystem::path path) {
  Directory dir;
  dir.fat_dir_ = std::make_unique<DIR>();
  ThrowIfError(f_opendir(dir.fat_dir_.get(), path.c_str()));
  return dir;
}

Directory::~Directory() {
  try {
    ThrowIfError(f_closedir(fat_dir_.get()));
  } catch (const std::filesystem::filesystem_error& e) {
    std::cout << "Error while closing directory: " << e.what() << std::endl;
  }
}

namespace {
Directory::Entry ToEntry(const FILINFO& info) {
  const std::string_view name = info.fname;
  return {.path = std::string_view(info.fname),
          .is_directory = static_cast<bool>(info.fattrib & AM_DIR)};
}
}  // namespace

std::vector<Directory::Entry> Directory::Entries() {
  std::vector<Entry> entries;
  FILINFO file_info;
  while (true) {
    ThrowIfError(f_readdir(fat_dir_.get(), &file_info));
    if (file_info.fname[0] == 0) {
      // End of directory.
      ThrowIfError(f_rewinddir(fat_dir_.get()));
      return entries;
    }
    entries.push_back(ToEntry(file_info));
  }
}
