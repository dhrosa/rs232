#include "flash.h"

#include <hardware/flash.h>
#include <hardware/sync.h>

#include <sstream>
#include <stdexcept>
#include <string>

FlashDisk::FlashDisk(int sector_count) {
  const std::span<const Sector> flash(reinterpret_cast<const Sector*>(XIP_BASE),
                                      PICO_FLASH_SIZE_BYTES / kSectorSize);

  sectors_ = flash.last(sector_count);
}

const FlashDisk::Sector& FlashDisk::ReadSector(int i) {
  CheckInRange(i);
  return sectors_[i];
}

void FlashDisk::WriteSector(int i, std::span<const std::uint8_t> payload) {
  CheckInRange(i);
  if (payload.size() != kSectorSize) {
    std::stringstream ss;
    ss << "Payload size does not match flash sector size: " << payload.size()
       << " vs " << kSectorSize;
    throw std::length_error(ss.str());
  }
  const auto interrupts = save_and_disable_interrupts();
  const uint32_t offset = i * kSectorSize;
  flash_range_erase(offset, kSectorSize);
  flash_range_program(offset, payload.data(), kSectorSize);
  restore_interrupts(interrupts);
}

void FlashDisk::CheckInRange(int i) {
  if (i >= 0 && i < sectors_.size()) {
    return;
  }
  std::stringstream ss;
  ss << "Flash sector index " << i << " is outside of valid range [0, "
     << sectors_.size() << ")";
  throw std::out_of_range(ss.str());
}
