#pragma once

#include <cstdint>
#include <span>

class FlashDisk {
 public:
  static constexpr int kSectorSize = 4096;
  using Sector = std::uint8_t[kSectorSize];

  // Uses the end of flash memory as storage.
  FlashDisk(int sector_count);

  const Sector& ReadSector(int i);

  void WriteSector(int i, std::span<const std::uint8_t> payload);

  std::size_t SectorCount() { return sectors_.size(); }

 private:
  void CheckInRange(int i);

  std::span<const Sector> sectors_;
};
