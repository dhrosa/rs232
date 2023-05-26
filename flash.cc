#include "flash.h"

#include <fmt/core.h>
#include <hardware/flash.h>
#include <hardware/sync.h>

#include <algorithm>
#include <stdexcept>

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
    throw std::length_error(
        fmt::format("Payload size does not match flash sector size: {} vs {}",
                    payload.size(), kSectorSize));
  }
  const Sector& dest = sectors_[i];
  const std::span<const std::byte, kSectorSize> src(payload.data(),
                                                    kSectorSize);

  // See how much of the source already matches the destination to try to avoid
  // an unneccessary sector erase.
  const uint32_t unaligned_mismatch_offset =
      std::ranges::mismatch(dest, src).in2 - src.begin();
  // Find the page-aligned subspan of the source that doesn't match.
  const std::span<const std::byte> src_mismatch =
      src.subspan((unaligned_mismatch_offset / kPageSize) * kPageSize);
  if (src_mismatch.empty()) {
    // Source and destination already match; no write needed at all.
    return;
  }

  // Offset from start of flash.
  const uint32_t offset =
      // Offset from start of flash to start of sector
      (sectors_.data() - flash.data() + i) * kSectorSize +
      // Offset from start of sector to first mismatched page.
      (src_mismatch.data() - src.data());

  const auto interrupts = save_and_disable_interrupts();
  if (src_mismatch.data() == src.data()) {
    // All pages differ between source and destination; we need to erase the
    // entire sector. In this case, the computed offset will already be aligned
    // to a sector boundary.
    flash_range_erase(offset, kSectorSize);
  }
  // Write all differing pages.
  flash_range_program(offset,
                      reinterpret_cast<const uint8_t*>(src_mismatch.data()),
                      src_mismatch.size());
  restore_interrupts(interrupts);
}

void FlashDisk::CheckInRange(int i) {
  if (i >= 0 && i < sectors_.size()) {
    return;
  }
  throw std::out_of_range(
      fmt::format("Flash sector index {} is out of valid range [0, {})", i,
                  sectors_.size()));
}
