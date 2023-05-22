#include "msc_device.h"

#include <cstring>
#include <iostream>

#include "usb_device.h"

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  return UsbDevice::Instance().Msc(lun).Ready();
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
  MscDevice& device = UsbDevice::Instance().Msc(lun);
  auto set = [&](uint8_t* dest, std::string_view str) {
    for (char c : str) {
      *(dest++) = c;
    }
    *dest = 0;
  };

  set(vendor_id, device.VendorId());
  set(product_id, device.ProductId());
  set(product_rev, device.ProductRev());
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count,
                         uint16_t* block_size) {
  MscDevice& device = UsbDevice::Instance().Msc(lun);
  *block_count = device.Disk().SectorCount();
  *block_size = device.Disk().kSectorSize;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject) {
  std::cout << "MSC Start Stop Unit command: start=" << start << " "
            << "load_eject=" << load_eject << std::endl;
  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void* buffer, uint32_t count) {
  MscDevice& device = UsbDevice::Instance().Msc(lun);
  std::memcpy(buffer, device.Disk().ReadSector(lba).data() + offset, count);
  return count;
}

bool tud_msc_is_writeable_cb(uint8_t lun) { return false; }

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t* buffer, uint32_t count) {
  return -1;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                        uint16_t count) {
  const uint8_t op = scsi_cmd[0];
  switch (op) {
    case 0x1E:
      return 0;
    default:
      std::cout << "Unsupported SCSI operation: " << static_cast<int>(op)
                << std::endl;
      return -1;
  }
}
