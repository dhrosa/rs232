#include "flash.h"

#include <hardware/flash.h>
#include <hardware/sync.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {
const auto flash =
    std::span(reinterpret_cast<const FlashDisk::Sector*>(XIP_BASE),
              PICO_FLASH_SIZE_BYTES / FlashDisk::kSectorSize);

}  // namespace

FlashDisk::FlashDisk(int sector_count) { sectors_ = flash.last(sector_count); }

const FlashDisk::Sector& FlashDisk::ReadSector(int i) {
  CheckInRange(i);
  return sectors_[i];
}

void FlashDisk::WriteSector(int i, std::span<const std::byte> payload) {
  CheckInRange(i);
  if (payload.size() != kSectorSize) {
    std::stringstream ss;
    ss << "Payload size does not match flash sector size: " << payload.size()
       << " vs " << kSectorSize;
    throw std::length_error(ss.str());
  }
  const uint32_t offset = (sectors_.data() - flash.data() + i) * kSectorSize;
  const auto interrupts = save_and_disable_interrupts();
  flash_range_erase(offset, kSectorSize);
  flash_range_program(offset, reinterpret_cast<const uint8_t*>(payload.data()),
                      kSectorSize);
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
