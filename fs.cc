#include "fs.h"

// clang-format off
#include <ff.h>
#include <diskio.h>
// clang-format on

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "flash.h"

namespace {
constexpr int kSectorSize = FlashDisk::kSectorSize;
FlashDisk* g_disk;

bool fs_initialized = false;
}  // namespace

///////////////////////////
// TinyUSB MSC callbacks //
///////////////////////////

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
      *reinterpret_cast<LBA_t*>(buffer) = g_disk->SectorCount();
      return RES_OK;
    }
    case GET_BLOCK_SIZE: {
      *reinterpret_cast<DWORD*>(buffer) = 1;
      return RES_OK;
    }
    default:
      std::cout << fmt::format("Unsupported disk_ioctl command: {}", command)
                << std::endl;
      return RES_PARERR;
  }
  return RES_OK;
}

DRESULT disk_read(BYTE drive, BYTE* buffer, LBA_t start_sector,
                  UINT sector_count) {
  auto* out = reinterpret_cast<FlashDisk::Sector*>(buffer);
  for (int i = 0; i < sector_count; ++i) {
    std::memcpy(*out++, g_disk->ReadSector(start_sector + i), kSectorSize);
  }
  return RES_OK;
}

DRESULT disk_write(BYTE drive, const BYTE* buffer, LBA_t start_sector,
                   UINT sector_count) {
  auto* in = reinterpret_cast<const FlashDisk::Sector*>(buffer);
  for (int i = 0; i < sector_count; ++i) {
    g_disk->WriteSector(start_sector + i, *(in++));
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

struct Error {
  std::string_view name;
  std::errc errc = std::errc::io_error;
};

Error ToError(FRESULT result) {
  switch (result) {
    case FR_DISK_ERR:
      return {"DISK_ERR"};
    case FR_INT_ERR:
      return {"INT_ERR"};
    case FR_NOT_READY:
      return {"NOT_READY"};
    case FR_NO_FILE:
      return {"NO_FILE", std::errc::no_such_file_or_directory};
    case FR_NO_PATH:
      return {"NO_PATH", std::errc::no_such_file_or_directory};
    case FR_INVALID_NAME:
      return {"INVALID_NAME", std::errc::invalid_argument};
    case FR_DENIED:
      return {"DENIED", std::errc::permission_denied};
    case FR_EXIST:
      return {"EXIST", std::errc::file_exists};
    case FR_INVALID_OBJECT:
      return {"INVALID_OBJECT", std::errc::invalid_argument};
    case FR_WRITE_PROTECTED:
      return {"WRITE_PROTECTED", std::errc::read_only_file_system};
    case FR_INVALID_DRIVE:
      return {"INVALID_DRIVE", std::errc::no_such_device};
    case FR_NOT_ENABLED:
      return {"NOT_ENABLED", std::errc::no_such_device};
    case FR_NO_FILESYSTEM:
      return {"NO_FILESYSTEM", std::errc::no_such_device};
    case FR_MKFS_ABORTED:
      return {"MKFS_ABORTED"};
    case FR_TIMEOUT:
      return {"TIMEOUT", std::errc::timed_out};
    case FR_LOCKED:
      return {"LOCKED", std::errc::no_lock_available};
    case FR_NOT_ENOUGH_CORE:
      return {"NOT_ENOUGH_CORE", std::errc::not_enough_memory};
    case FR_TOO_MANY_OPEN_FILES:
      return {"TOO_MANY_OPEN_FILES", std::errc::too_many_files_open};
    case FR_INVALID_PARAMETER:
      return {"INVALID_PARAMETER", std::errc::invalid_argument};
    default:
      return {"UNKNOWN"};
  }
}

void ThrowIfError(std::string_view op, FRESULT result) {
  if (result == FR_OK) {
    return;
  }
  const Error error = ToError(result);
  throw std::filesystem::filesystem_error(
      fmt::format("{} error: {} ({})", op, error.name, fmt::underlying(result)),
      std::make_error_code(error.errc));
}

void CreateFileSystem() {
  const LBA_t partition_sizes[] = {g_disk->SectorCount() - 5};
  std::array<BYTE, kSectorSize> work_area;
  ThrowIfError("fdisk", f_fdisk(0, partition_sizes, work_area.data()));
  ThrowIfError("mkfs",
               f_mkfs("0:", nullptr, work_area.data(), work_area.size()));
}
}  // namespace

void FileSystem::Install() {
  g_disk = &disk_;

  std::cout << "FAT file system initialization start." << std::endl;
  if (FRESULT result = f_mount(&fs_, "", 1); result == FR_NO_FILESYSTEM) {
    std::cout << "No valid FAT filesystem found. Attempting to create it."
              << std::endl;
    CreateFileSystem();
    ThrowIfError("mount", f_mount(&fs_, "", 1));
  } else {
    ThrowIfError("mount", result);
    std::cout << "Reusing existing FAT filesystem." << std::endl;
  }
  std::cout << "FAT file system initialization complete." << std::endl;
  fs_initialized = true;
}

File FileSystem::OpenFile(std::filesystem::path path, const OpenFlags& flags) {
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
  ThrowIfError("open", f_open(file.fat_file_.get(), path.c_str(), mode));
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
  ThrowIfError("close", f_close(std::exchange(fat_file_, nullptr).get()));
}

int File::Size() {
  return f_size(fat_file_.get());
}

int File::Tell() { return f_tell(fat_file_.get()); }

void File::Seek(int location) {
  ThrowIfError("lseek", f_lseek(fat_file_.get(), location));
}

std::span<std::byte> File::Read(std::span<std::byte> buffer) {
  UINT bytes_read;
  ThrowIfError("read", f_read(fat_file_.get(), buffer.data(), buffer.size(),
                              &bytes_read));
  return buffer.subspan(bytes_read);
}

std::string File::ReadAll() {
  std::string str(Size(), 0);
  Read(std::as_writable_bytes(std::span(str)));
  return str;
}

int File::Write(std::span<const std::byte> buffer) {
  UINT bytes_written;
  ThrowIfError("write", f_write(fat_file_.get(), buffer.data(), buffer.size(),
                                &bytes_written));
  return bytes_written;
}

int File::Write(std::string_view str) {
  return Write(std::as_bytes(std::span(str)));
}

void File::Sync() { ThrowIfError("sync", f_sync(fat_file_.get())); }

Directory FileSystem::OpenDirectory(std::filesystem::path path) {
  Directory dir;
  dir.fat_dir_ = std::make_unique<DIR>();
  ThrowIfError("opendir", f_opendir(dir.fat_dir_.get(), path.c_str()));
  return dir;
}

Directory::~Directory() {
  try {
    ThrowIfError("closedir", f_closedir(fat_dir_.get()));
  } catch (const std::filesystem::filesystem_error& e) {
    std::cout << "Error while closing directory: " << e.what() << std::endl;
  }
}

std::vector<Directory::Entry> Directory::Entries() {
  std::vector<Entry> entries;
  FILINFO info;
  while (true) {
    ThrowIfError("readdir", f_readdir(fat_dir_.get(), &info));
    const std::string_view path(info.fname);
    if (path.empty()) {
      // End of directory.
      ThrowIfError("rewinddir", f_rewinddir(fat_dir_.get()));
      return entries;
    }
    entries.push_back(
        Entry{.path = path,
              .is_directory = static_cast<bool>(info.fattrib & AM_DIR)});
  }
}
