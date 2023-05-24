#pragma once

#include <hardware/flash.h>

#include <cstdint>
#include <span>

class FlashDisk {
 public:
  static constexpr int kSectorSize = FLASH_SECTOR_SIZE;
  using Sector = std::byte[kSectorSize];

  // Uses the end of flash memory as storage.
  FlashDisk(int sector_count);

  const Sector& ReadSector(int i);

  void WriteSector(int i, std::span<const std::byte> payload);

  std::size_t SectorCount() { return sectors_.size(); }

 private:
  void CheckInRange(int i);

  std::span<const Sector> sectors_;
};
